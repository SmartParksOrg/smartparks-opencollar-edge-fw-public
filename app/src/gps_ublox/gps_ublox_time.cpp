#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>

#include "gps_ublox.h"
#include "gps_ublox_interface.h"

extern "C" {
#include "global_time.h"
}

LOG_MODULE_REGISTER(gps_ublox_time, 3);

extern SFE_UBLOX_GPS myGPS;

/* UBX-NAV-PVT validity bits, fix-status flag and accepted fix-type bounds. */
#define UBX_NAV_PVT_VALID_DATE     0x01
#define UBX_NAV_PVT_VALID_TIME     0x02
#define UBX_NAV_PVT_FULLY_RESOLVED 0x04
#define UBX_NAV_PVT_REQUIRED_TIME_VALIDITY                                                         \
	(UBX_NAV_PVT_VALID_DATE | UBX_NAV_PVT_VALID_TIME | UBX_NAV_PVT_FULLY_RESOLVED)
#define UBX_NAV_PVT_GNSS_FIX_OK   0x01
#define UBX_NAV_PVT_FIX_2D        2
#define UBX_NAV_PVT_FIX_TIME_ONLY 5

static constexpr uint32_t MAX_TIME_ACCURACY_NS = 1000000000U;

/* A large correction needs three distinct epochs in the same acquisition attempt. */
static constexpr int64_t LARGE_CORRECTION_SECONDS = 30;
static constexpr unsigned int CONFIRMATION_SAMPLES = 3;
static constexpr int64_t CONSISTENCY_TOLERANCE_MS = 1500;
static constexpr uint32_t GPS_WEEK_MS = 604800000;

/**
 * @brief Read one fresh NAV-PVT epoch and validate its UTC time before conversion.
 *
 * Check the epoch's validity flags, fix status, time accuracy and calendar fields.
 *
 * @param[out] sample Received snapshot. May be populated even if validation fails.
 * @param[out] timestamp Validated Unix time in seconds. Unchanged on failure.
 * @retval 0 A valid UTC timestamp was obtained.
 * @retval -EIO A fresh NAV-PVT snapshot could not be read.
 * @retval -EAGAIN The epoch's validity, fix, accuracy or time-of-week checks failed.
 * @retval -ERANGE The calendar fields or converted Unix timestamp are out of range.
 */
static int prv_read_time(ublox_time_solution *sample, uint32_t *timestamp)
{
	if (!myGPS.getTimeSolution(sample)) {
		return -EIO;
	}

	/* validDate, validTime and fullyResolved must qualify this exact epoch.
	 * Accept position or time-only fixes, but not an unqualified no-fix estimate.
	 */
	if ((sample->valid & UBX_NAV_PVT_REQUIRED_TIME_VALIDITY) !=
		    UBX_NAV_PVT_REQUIRED_TIME_VALIDITY ||
	    !(sample->flags & UBX_NAV_PVT_GNSS_FIX_OK) || sample->fix_type < UBX_NAV_PVT_FIX_2D ||
	    sample->fix_type > UBX_NAV_PVT_FIX_TIME_ONLY ||
	    sample->time_accuracy_ns > MAX_TIME_ACCURACY_NS ||
	    sample->time_of_week_ms >= GPS_WEEK_MS) {
		return -EAGAIN;
	}

	static const uint8_t month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	/* uint32_t Unix timestamps end at 2106-02-07 06:28:15 UTC. Before 2106, migrate
	 * timestamp storage and message formats to 64 bits and update these range checks.
	 */
	if (sample->year < 1970 || sample->year > 2106 || sample->month < 1 || sample->month > 12 ||
	    sample->hour > 23 || sample->minute > 59 || sample->second > 59) {
		/* Defer leap-second epochs until UTC is representable as Unix seconds. */
		return -ERANGE;
	}
	unsigned int days = month_days[sample->month - 1];
	bool leap = sample->year % 4 == 0 && (sample->year % 100 != 0 || sample->year % 400 == 0);
	if (sample->month == 2 && leap) {
		days++;
	}
	if (sample->day < 1 || sample->day > days) {
		return -ERANGE;
	}

	struct tm utc = {};
	utc.tm_year = sample->year - 1900;
	utc.tm_mon = sample->month - 1;
	utc.tm_mday = sample->day;
	utc.tm_hour = sample->hour;
	utc.tm_min = sample->minute;
	utc.tm_sec = sample->second;
	int64_t seconds = timeutil_timegm64(&utc);
	if (seconds <= 0 || seconds > UINT32_MAX) {
		return -ERANGE;
	}
	*timestamp = (uint32_t)seconds;
	return 0;
}

/**
 * @brief Get a validated GPS UTC timestamp suitable for updating the clock reference.
 *
 * Corrections larger than 30 seconds require three distinct valid epochs whose UTC
 * and GPS time-of-week progression agree with uptime. Confirmation sleeps for one
 * second before each additional read. The caller applies the accepted reference.
 *
 * @param[out] timestamp Validated Unix time in seconds. Unchanged on failure.
 * @retval 0 A timestamp passed validation and any required confirmation.
 * @retval -EINVAL timestamp is null.
 * @retval -EIO A fresh NAV-PVT snapshot could not be read.
 * @retval -EAGAIN An epoch failed quality checks or confirmation was inconsistent.
 * @retval -ERANGE The calendar fields or converted Unix timestamp are out of range.
 */
int gps_ublox_get_datetime(uint32_t *timestamp)
{
	if (timestamp == nullptr) {
		return -EINVAL;
	}
	ublox_time_solution first;
	uint32_t first_time;
	int err = prv_read_time(&first, &first_time);
	if (err) {
		return err;
	}
	int64_t correction = (int64_t)first_time - get_global_unix_time();
	uint32_t verified_time = first_time;
	if (correction > LARGE_CORRECTION_SECONDS || correction < -LARGE_CORRECTION_SECONDS) {
		LOG_WRN("Verifying GPS clock correction: %lld s", (long long)correction);
		uint32_t previous_time = first_time;
		uint32_t previous_tow = first.time_of_week_ms;
		for (unsigned int i = 1; i < CONFIRMATION_SAMPLES; i++) {
			k_sleep(K_SECONDS(1));
			ublox_time_solution next;
			err = prv_read_time(&next, &verified_time);
			if (err) {
				return err;
			}
			int64_t elapsed_ms = next.received_ms - first.received_ms;
			int64_t utc_ms = ((int64_t)verified_time - first_time) * 1000;
			int64_t tow_ms =
				(next.time_of_week_ms + GPS_WEEK_MS - first.time_of_week_ms) %
				GPS_WEEK_MS;
			if (verified_time <= previous_time ||
			    next.time_of_week_ms == previous_tow || elapsed_ms <= 0 ||
			    elapsed_ms > 10000 ||
			    llabs(utc_ms - elapsed_ms) > CONSISTENCY_TOLERANCE_MS ||
			    llabs(tow_ms - elapsed_ms) > CONSISTENCY_TOLERANCE_MS) {
				LOG_WRN("Rejected inconsistent GPS clock correction");
				return -EAGAIN;
			}
			previous_time = verified_time;
			previous_tow = next.time_of_week_ms;
		}
	}
	/* Do not expose a rejected candidate to callers or persistent storage. */
	*timestamp = verified_time;
	return 0;
}
