/*
    This is an interface that connects the CPP Ublox library with the main C code.
    Added to make it possible to run Ublox lib on Zephyr (NCS)

    This port was made by Vid Rajtmajer <vid@irnas.eu>, IRNAS d.o.o.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "generated_settings.h"
#include "nvs_storage.h"

#include "gps_ublox.h"
#include "gps_ublox_interface.h"

LOG_MODULE_REGISTER(gps_ublox_interface, 3); // init logging

SFE_UBLOX_GPS myGPS;   // driver class instance
uint64_t lastTime = 0; // Simple local timer. Limits amount if I2C traffic to Ublox module.

/* PRIVATE FUNCTIONS */
// Convert datetime string to unix timestamp (time_t)
uint32_t gps_ublox_convert_datetime_to_unix(char *timestamp_str)
{
	struct tm tm;
	uint32_t seconds;
	int r;

	if (timestamp_str == NULL) {
		LOG_ERR("Null argument");
		return 0;
	}
	r = sscanf(timestamp_str, "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
		   &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
	if (r != 6) {
		LOG_ERR("Expected %d numbers scanned in %s", r, timestamp_str);
		return 0;
	}

	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	tm.tm_isdst = 0;
	seconds = mktime(&tm);
	if (seconds == (uint32_t)-1) {
		LOG_ERR("Reading time from %s failed", timestamp_str);
		return 0;
	}
	return seconds;
}

/**
 * @brief Initialize I2C as Ublox communication device.
 *
 * @param gps_dev
 * @return int
 */
int gps_ublox_begin_i2c(const struct device *gps_dev)
{
	commTypes com = COMM_TYPE_I2C;
	if (myGPS.begin(*gps_dev, com) == false) {
		return -EIO;
	}
	k_sleep(K_MSEC(100));
	LOG_INF("Set navigation freq: %d", myGPS.getNavigationFrequency());
	return 0;
}

/**
 * @brief Initialize UART as Ublox communication device
 *
 * @param gps_dev
 * @return int
 */
int gps_ublox_begin_serial(const struct device *gps_dev)
{
	commTypes com = COMM_TYPE_SERIAL;
	if (myGPS.begin(*gps_dev, com) == false) {
		return -EIO;
	}
	return 0;
}

/**
 * @brief Enable processing of NMEA messages - needed for satellite data.
 *
 */
void gps_ublox_process_nmea_sentences(void)
{
	myGPS.setNMEAOutputPort();
	// myGPS.enableNMEAMessage(UBX_NMEA_GSV, COM_PORT_I2C);
	// myGPS.disableNMEAMessage(UBX_NMEA_GSV, COM_PORT_I2C);
}

/**
 * @brief Get new position data.
 *
 * @param[in] position
 * @return int
 */
int gps_ublox_get_position(struct gps_ublox_position_data *position)
{
	// Query module only every second. Doing it more often will just cause I2C traffic.
	// The module only responds when a new position is available, print it to console
	if (k_uptime_get() - lastTime > 1000) {
		memset(position, 0, sizeof(struct gps_ublox_position_data));

		lastTime = k_uptime_get(); // Update the timer
		// myGPS.flushPVT();

		/* Local variables */
		int32_t latitude = myGPS.getLatitude();
		int32_t longitude = myGPS.getLongitude();
		int32_t altitude = myGPS.getAltitude();
		uint8_t SIV = myGPS.getSIV();
		uint8_t fix_type = myGPS.getFixType();
		uint32_t accuracy =
			myGPS.getHorizontalAccuracy(); // 10-3 or 10-4 m ? Datasheet and driver do
						       // not agree... i.e. 0.1 mm precision
		/* For some reason to large value is sometimes obtained */
		if (accuracy >= UINT32_MAX) {
			accuracy = 0;
		}
		uint16_t scaled_accuracy = (uint16_t)(accuracy / 1000);
		uint16_t PDOP = myGPS.getPDOP(); // 10-2 value
		int32_t COG = myGPS.getHeading();
		uint16_t scaled_cog = (uint16_t)(COG / 1000 + 18000);
		int32_t SOG = myGPS.getGroundSpeed();

		LOG_INF("Position: Lat: %d, Lon: %d, Alt: %d, SIV: %d acc: %d fix type: %d PDOP: "
			"%d COG: %d SOG: %d",
			latitude, longitude, altitude, SIV, accuracy, fix_type, PDOP, COG, SOG);

		// Copy fix data
		position->fix_type = fix_type;
		position->SIV = SIV;
		position->scaled_accuracy = scaled_accuracy;
		position->PDOP = (uint8_t)(PDOP / 100);
		position->scaled_cog = scaled_cog;
		position->scaled_sog = (uint8_t)(SOG / 1000);

		// Check GPS fix type 0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix
		if (fix_type == 3 || fix_type == 2) {

			position->latitude = latitude;
			position->longitude = longitude;
			position->altitude = altitude;

			// Check accuracy
			if (accuracy > 0 &&
			    accuracy < Main_settings.horizontal_accuracy->def_val * 1000) {
				// Check fake detection
				if (latitude != 0 && longitude != 0 && altitude != 0) {
					Main_values.gps_lat->def_val = latitude;
					Main_values.gps_lon->def_val = longitude;
					Main_values.gps_alt->def_val = altitude;
					Main_values.gps_h_acc_est->def_val = scaled_accuracy;

					LOG_INF("New valid position obtained!");
					return 0;
				}
			}
			LOG_ERR("Did not get valid position!");
			return -ENOTSUP;
		}
		LOG_ERR("Did not get valid fix type: %d!", fix_type);
		return -ENOTSUP;
	}

	return -EBUSY;
}

/**
 * @brief Get the datetime object
 *
 * @return unix timestamp
 */
int gps_ublox_get_datetime(uint32_t *timestamp)
{
	int year = myGPS.getYear();
	int month = myGPS.getMonth();
	int day = myGPS.getDay();
	int hour = myGPS.getHour();
	int minute = myGPS.getMinute();
	int second = myGPS.getSecond();

	char datetime_str[26];
	sprintf(datetime_str, "%d-%d-%d %d:%d:%d", year, month, day, hour, minute, second);
	LOG_INF("DateTime: %s", datetime_str);

	if (myGPS.getTimeValid() == false) {
		LOG_ERR("Time is not valid");
		return -EIO;
	}

	if (myGPS.getDateValid() == false) {
		LOG_ERR("Date is not valid");
		return -EIO;
	}

	if (myGPS.getFullyResolved() == false) {
		LOG_ERR("GPS is not fully resolved");
		return -EIO;
	}

	// unix time return
	*timestamp = gps_ublox_convert_datetime_to_unix(datetime_str);

	return 0;
}

/**
 * @brief Get payload containing satellite data
 *
 * @param payload
 * @param max_payload
 * @return int
 */
int gps_ublox_get_sat_data(uint8_t *payload, uint8_t max_payload)
{
	return myGPS.getNMEADetectedSatelliteData(payload, max_payload);
}

/**
 * @brief Get number of currently detected satellites.
 *
 * @return int number of currently detected satellites.
 */
uint8_t gps_ublox_number_of_currently_detected_satellites(void)
{
	LOG_DBG("Number of currently detected satellites: %d",
		myGPS.getNMEANumberOfCurrentlyDetectedSatellites());
	return myGPS.getNMEANumberOfCurrentlyDetectedSatellites();
}

/**
 * @brief Clear buffer, flush position and time data and reset satellite data structure.
 *
 */
void gps_ublox_flush(void)
{
	gps_ublox_clear_buffer();
	gps_ublox_flush_data();
	gps_ublox_reset_sat_data();
}

/**
 * @brief Clear I2C buffer.
 *
 */
void gps_ublox_clear_buffer(void)
{
	myGPS.clearBuffer();
}

/**
 * @brief Mark all data as stale.
 *
 */
void gps_ublox_flush_data(void)
{
	myGPS.flushPVT();
}

/**
 * @brief Reset structure containing satellide data
 *
 */
void gps_ublox_reset_sat_data(void)
{
	myGPS.reset_NMEA_detected_satellite_data();
}

/**
 * @brief Perform hard reset of Ublox module.
 *
 */
void gps_ublox_reset(void)
{
	myGPS.hardReset();
}

void gps_ublox_print_position_data(struct gps_ublox_position_data *position)
{
	LOG_INF("Position: lat: %d, lon: %d, alt: %d, SIV: %d, fix_type: %d, scaled_accuracy: %d, "
		"PDOP: %d, scaled_cog: %d, scaled_sog: %d",
		position->latitude, position->longitude, position->altitude, position->SIV,
		position->fix_type, position->scaled_accuracy, position->PDOP, position->scaled_cog,
		position->scaled_sog);
}
