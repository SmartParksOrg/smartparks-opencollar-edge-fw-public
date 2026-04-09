/*
    gps.c - Ublox gps wrapper - this is called from main
    It inits GPIO and desired I2C.
    Then it calls gps_ublox_interface for init, get position and get time.
    Made by Vid Rajtmajer <vid@irnas.eu>, IRNAS d.o.o.
*/
#include <errno.h>
#include <time.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/types.h>

#include "generated_settings.h"
#include "global_time.h"
#include "gps.h"
#include "gps_ublox_interface.h"
#include "nvs_storage.h"
#include "status.h"

#include "led.h"

#define GPS_PWR_NODE DT_NODELABEL(gps_pwr)
#if DT_NODE_EXISTS(GPS_PWR_NODE)
const struct gpio_dt_spec gpio_dev_pow = GPIO_DT_SPEC_GET(GPS_PWR_NODE, gpios);
#endif /* DT_NODE_EXISTS(GPS_PWR_NODE) */

#define GPS_VBCK_NODE DT_NODELABEL(gps_vbck)
#if DT_NODE_EXISTS(GPS_VBCK_NODE)
const struct gpio_dt_spec gpio_dev_vbck = GPIO_DT_SPEC_GET(GPS_VBCK_NODE, gpios);
#endif /* DT_NODE_EXISTS(GPS_VBCK_NODE) */

#define GPS_I2C  DT_ALIAS(gps_i2c)
#define GPS_UART DT_ALIAS(gps_uart)

#if DT_NODE_EXISTS(GPS_I2C)
#define GPS_SDA DT_PROP(GPS_I2C, sda_pin)
#define GPS_SCL DT_PROP(GPS_I2C, scl_pin)
const struct device *gps_dev = DEVICE_DT_GET(GPS_I2C);
#elif DT_NODE_EXISTS(GPS_UART)
const struct device *gps_dev = DEVICE_DT_GET(GPS_UART);
#else
const struct device *gps_dev;
#endif

#define RETRY_INTERVAL 1000

/**
 * @brief Minimum allowed ublox fix interval, that will not interfere with other device processes.
 */
#define FIX_MINIMUM_INTERVAL 30

/**
 * @brief Maximum number of motion triggered GPS events in one gps trigger interval
 * (`gps_triggered_interval`). Used for motion triggered GPS fix where we wait for a certain number
 * of motion events before getting a new fix.
 */
#define GPS_MOTION_DETECTION_MAX_NUMBER_OF_EVENTS 255

LOG_MODULE_REGISTER(gps, 3); // init logging

/**
 * @brief Array of timestamps of motion detection events.
 *
 * IMPORTANT: Events must always maintain chronological order! Older events are at the
 * beginning of the array, followed by newer events.
 */
static uint32_t prv_gps_motion_event_timestamps[GPS_MOTION_DETECTION_MAX_NUMBER_OF_EVENTS] = {0};
static int prv_gps_motion_event_timestamp_idx = 0;
static bool prv_gps_motion_get_fix = false;

static uint64_t prv_time_of_last_check_min_number_of_satellites = 0;

static int64_t prv_last_gps_coldfix = 0;

enum comDevType {
	I2C_DEV = 0,
	SERIAL_DEV = 1
};

const struct uart_config uart_cfg = {
	.baudrate = 9600,
	.parity = UART_CFG_PARITY_NONE,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.data_bits = UART_CFG_DATA_BITS_8,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
};

struct context {
	/* Communication channel */
	enum comDevType com;
	/* Module enabled flag */
	bool enabled;
	/* Active tracking flag */
	bool active_tracking;

	/* Initial cold fix flag */
	bool cold_fix;
	/* Recent hot fix flag */
	bool hot_fix;
	/* Number of cold fix retries */
	int cold_retry;
	/* Number of hot fix retries */
	int hot_retry;
	/* Last fix success */
	bool last_fix_success;

	/* Fix start uptime */
	uint64_t fix_start;
	/* Time of last fix try */
	uint64_t last_fix_try;
	/* Last fix time */
	uint16_t ttf;

	/* Backoff settings */

	/* Defout sending interval - without backoff modifications */
	uint32_t default_send_interval;
	/* Currently used sending interval */
	uint32_t send_interval;

	/* Two intervals settings - currently used interval */
	uint8_t interval_type;

	/* Motion triggered gps variables - number of shipped attempts */
	uint8_t skipped_attempts;

	/* Last fix position data */
	struct gps_ublox_position_data position;

	/* Last fix time data */
	uint32_t fix_time;
};

static struct context prv_ctx = {.com = I2C_DEV,
				 .enabled = false,
				 .active_tracking = false,
				 .cold_fix = false,
				 .hot_fix = false,
				 .cold_retry = 0,
				 .hot_retry = 0,
				 .fix_start = 0,
				 .last_fix_try = 0,
				 .ttf = 0,
				 .default_send_interval = 0,
				 .send_interval = 0,
				 .interval_type = 1,
				 .skipped_attempts = 0};

/* PRIVATE FUNCTIONS */

/**
 * @brief Clear all motion detection event timestamps.
 */
static void prv_gps_motion_detection_clear_all_timestamps(void)
{
	memset(&prv_gps_motion_event_timestamps[0], 0, sizeof(prv_gps_motion_event_timestamps));
	prv_gps_motion_event_timestamp_idx = 0;
}

/**
 * @brief Check if motion detection counter equals or exceeds the minimum number of triggers per
 * interval.
 *
 * @return true counter equal or exceeds the minimum number of triggers per interval.
 * @return false counter does not equal or exceed the minimum number of triggers per interval.
 */
static bool prv_gps_check_motion_detection_counter(void)
{
	/* Check if enabled */
	if (Main_settings.enable_motion_trig_gps->def_val == false ||
	    Main_settings.gps_motion_triggered_min_num_of_triggers_per_interval->def_val == 0 ||
	    Main_settings.gps_triggered_interval->def_val == 0) {
		return false;
	}

	if (prv_gps_motion_get_fix) {
		prv_gps_motion_get_fix = false;
		return true;
	}
	return false;
}

/**
 * @brief Init GPIO devices for power and VBCK pins.
 *
 * @return int
 */
static int gps_init_gpio(void)
{
	int err = 0;

#if DT_NODE_EXISTS(GPS_PWR_NODE)
	gpio_pin_configure_dt(&gpio_dev_pow, GPIO_OUTPUT_ACTIVE);
	LOG_INF("GPS power gpio initialized!");
#else
	LOG_WRN("PWR GPS pin not defined!");
	err = -ENXIO;
#endif

#if DT_NODE_EXISTS(GPS_VBCK_NODE)
	gpio_pin_configure_dt(&gpio_dev_vbck, GPIO_OUTPUT_INACTIVE);
	LOG_INF("GPS vbck gpio initialized!");
#else
	LOG_WRN("VBCK GPS pin not defined!");
	err = -ENXIO;
#endif

	return err;
}

/**
 * @brief Init GPS device device, provided Ublox uses I2C or UART for communication.
 *
 * @return int
 */
static int gps_init_device(void)
{
#if DT_NODE_EXISTS(GPS_I2C)
	if (!gps_dev) {
		LOG_ERR("GPS device does not exists");
		return -EIO;
	} else {
		LOG_INF("GPS %s Init OK", gps_dev->name);
		prv_ctx.com = I2C_DEV;
		return 0;
	}
#elif DT_NODE_EXISTS(GPS_UART)
	if (!gps_dev) {
		LOG_ERR("GPS device does not exists");
		return -EIO;
	} else {
		LOG_INF("GPS %s Init OK", gps_dev->name);
		prv_ctx.com = SERIAL_DEV;
		// Configure
		int err = uart_configure(gps_dev, &uart_cfg);
		return err;
	}
#else
	LOG_WRN("No device supported for GPS!");
	return -ENXIO;
#endif
}

/**
 * @brief Disable Ublox communication.
 *
 */
static void gps_com_disable(void)
{
#ifdef CONFIG_LOW_POWER
	// Disable UART or I2C
	LOG_INF("Disable GPS com.\n");
	if (gps_dev) {
		pm_device_action_run(gps_dev, PM_DEVICE_ACTION_SUSPEND);
	}
#endif
	k_sleep(K_MSEC(100));
}

/**
 * @brief Enable Ublox communication.
 *
 */
static void gps_com_enable(void)
{
#ifdef CONFIG_LOW_POWER
	// Enable I2C or UART
	LOG_INF("Enable GPS com.\n");
	pm_device_action_run(gps_dev, PM_DEVICE_ACTION_RESUME);
#endif
	k_sleep(K_MSEC(100));
}

/**
 * @brief Obtain new data from Ublox module
 *
 * @return int
 */
static int gps_get_new_data(void)
{
	/* Get position */
	int err = 0;
	err = gps_ublox_get_position(&prv_ctx.position);
	if (!err) {
		nvs_storage_write(STORAGE_latitude, &Main_values.gps_lat->def_val,
				  sizeof(Main_values.gps_lat->def_val));
		nvs_storage_write(STORAGE_longitude, &Main_values.gps_lon->def_val,
				  sizeof(Main_values.gps_lon->def_val));
		nvs_storage_write(STORAGE_altitude, &Main_values.gps_alt->def_val,
				  sizeof(Main_values.gps_alt->def_val));
	}
	prv_ctx.last_fix_try = k_uptime_get();

	/* Get time and update reference */
	int time_err = gps_ublox_get_datetime(&prv_ctx.fix_time);
	if (time_err) {
		LOG_ERR("Got invalid gps time: %d, will not update unix time, taken old reference: "
			"%d!",
			prv_ctx.fix_time, Main_values.ublox_time->def_val);
		prv_ctx.fix_time = get_global_unix_time();
	} else {
		update_ref_time(prv_ctx.fix_time);
	}

	/* If we got new position, update last fix time */
	if (!err) {
		Main_values.last_position_time->def_val = get_global_unix_time();
	}

	return err;
}

/*!
 * @brief Check if send period was changed in settings
 *
 * @return void
 */
static void gps_update_send_interval(uint8_t interval_type)
{
	/* Set intervals */
	if (interval_type == 1) {
		if (prv_ctx.default_send_interval != Main_settings.ublox_send_interval->def_val) {
			prv_ctx.default_send_interval = Main_settings.ublox_send_interval->def_val;
		}
	} else {
		if (prv_ctx.default_send_interval != Main_settings.ublox_send_interval_2->def_val) {
			prv_ctx.default_send_interval =
				Main_settings.ublox_send_interval_2->def_val;
		}
	}
	/* If the send interval is non-zero and too small, set it to minimum allowed */
	if (prv_ctx.default_send_interval > 0 &&
	    prv_ctx.default_send_interval < FIX_MINIMUM_INTERVAL) {
		prv_ctx.default_send_interval = FIX_MINIMUM_INTERVAL;
		LOG_WRN("Send interval too low. Minimum non-zero send interval allowed is %d "
			"seconds! Setting new interval value.",
			FIX_MINIMUM_INTERVAL);
		if (interval_type == 1) {
			Main_settings.ublox_send_interval->def_val = FIX_MINIMUM_INTERVAL;
			nvs_storage_write(Main_settings.ublox_send_interval->id,
					  &Main_settings.ublox_send_interval->def_val,
					  Main_settings.ublox_send_interval->len);
		} else {
			Main_settings.ublox_send_interval_2->def_val = FIX_MINIMUM_INTERVAL;
			nvs_storage_write(Main_settings.ublox_send_interval_2->id,
					  &Main_settings.ublox_send_interval_2->def_val,
					  Main_settings.ublox_send_interval_2->len);
		}
	}
	prv_ctx.send_interval = prv_ctx.default_send_interval;
}

/*!
 * @brief Check in which time interval we are at the moment
 *
 * NOTE: If both intervals are set to the same hour, interval 1 is selected.
 *
 * The function checks if we are in-between or outside of the two intervals. The classification of
 * being in-between and outside of the two intervals is daily separated, meaning that in-between
 * represents the time that is between the two intervals on the same day, while outside represents
 * the time that is outside of the two intervals on the same day.
 *
 * If we are in-between the two intervals, the preceding interval is selected.
 *
 * If we are outside of the two intervals, the subsequent interval is selected.
 *
 * @return int interval 1 or 2
 */
static uint8_t gps_check_time_interval_type(void)
{
	uint8_t interval = 1;
	/* Check if switching intervals is active */
	if (Main_settings.ublox_multiple_intervals->def_val) {
		/* Get latest unix time */
		uint8_t hours = (get_global_unix_time() / 3600) % 24;

		/* NOTE: If both intervals are set to the same hour, interval 1 is selected */
		if (hours == Main_settings.ublox_interval2_start->def_val) {
			interval = 2;
		}
		if (hours == Main_settings.ublox_interval1_start->def_val) {
			interval = 1;
			return interval;
		}

		/* Check if we are outside the two intervals (circadianly - in the same day). */
		if ((hours < Main_settings.ublox_interval1_start->def_val &&
		     hours < Main_settings.ublox_interval2_start->def_val) ||
		    (hours > Main_settings.ublox_interval1_start->def_val &&
		     hours > Main_settings.ublox_interval2_start->def_val)) {
			/* Select subsequent interval */
			if (Main_settings.ublox_interval1_start->def_val >=
			    Main_settings.ublox_interval2_start->def_val) {
				interval = 1;
			} else {
				interval = 2;
			}
		}

		/* Check if we are in-between the two intervals (circadianly - in the same day) */
		if (hours > Main_settings.ublox_interval1_start->def_val &&
		    hours < Main_settings.ublox_interval2_start->def_val) {
			interval = 1;
		}
		if (hours < Main_settings.ublox_interval1_start->def_val &&
		    hours > Main_settings.ublox_interval2_start->def_val) {
			interval = 2;
		}
		LOG_DBG("Current hour: %d, we are in interval: %d", hours, interval);
	} else {
		interval = 1;
	}

	return interval;
}

/*!
 * @brief Check if active tracking flag was changed in settings
 *
 * @return void
 */
static void gps_check_active_tracking(void)
{
	if (prv_ctx.active_tracking != Main_settings.ublox_active_tracking->def_val) {
		prv_ctx.active_tracking = Main_settings.ublox_active_tracking->def_val;
		if (prv_ctx.active_tracking) {
			/* Enable communication */
			gps_com_enable();
		}
#if DT_NODE_EXISTS(GPS_PWR_NODE)
		LOG_INF("Set Ublox GPS power for active tracking: %d", prv_ctx.active_tracking);
		gpio_pin_set_dt(&gpio_dev_pow, (int)prv_ctx.active_tracking); /* GPS power */
#else
		LOG_WRN("GPS POWER pin not defined in DT!");
#endif

#if DT_NODE_EXISTS(GPS_VBCK_NODE)
		gpio_pin_set_dt(&gpio_dev_vbck, 1);
#endif
		k_sleep(K_MSEC(500));
		if (!prv_ctx.active_tracking) {
			// Disable UART or I2C
			gps_com_disable();
		}

		prv_ctx.cold_retry = 0;
		prv_ctx.cold_fix = false;
		prv_ctx.hot_retry = 0;
		prv_ctx.hot_fix = false;
	}
}

/**
 * @brief Check if we have at least the minimum number of satellites detected.
 *
 * @return true sufficient number of satellites detected.
 * @return false insufficient number of satellites detected.
 */
static bool gps_check_min_number_of_satellites(void)
{
	if (Main_settings.ublox_min_satellites->def_val == 0) {
		return true;
	}
	uint8_t num_sat = gps_ublox_number_of_currently_detected_satellites();
	if (num_sat < Main_settings.ublox_min_satellites->def_val) {
		LOG_WRN("Number of satellites: %d, below minimum: %d. Stopping fix.", num_sat,
			Main_settings.ublox_min_satellites->def_val);
		return false;
	}
	LOG_INF("Number of satellites is sufficient: %d. (Minimum: %d)", num_sat,
		Main_settings.ublox_min_satellites->def_val);
	return true;
}

/**
 * @brief Check if the minimum number of satellites check period elapsed run check.
 *
 * @param fix_start time of fix start
 * @retval 0 if we have sufficient number of satellites detected.
 * @retval -ECANCELED not enough time elapsed from last check.
 * @retval -EIO insufficient number of satellites detected.
 */
static int prv_min_number_of_satellites_checker(void)
{
	/* Periodically check if we have enough satellites for fix */
	if ((uint32_t)((k_uptime_get() - prv_time_of_last_check_min_number_of_satellites) / 1000) >
	    Main_settings.ublox_min_satellites_timer->def_val) {
		LOG_INF("Checking number of satellites.");
		prv_time_of_last_check_min_number_of_satellites = k_uptime_get();
		if (gps_check_min_number_of_satellites()) {
			return 0; /* Sufficient number of satellites */
		} else {
			return -EIO; /* Insufficient number of satellites */
		}
	}
	return -ECANCELED;
}

/**
 * @brief Check if the cold fix interval expired.
 *
 * @retval true if cold fix interval expired
 * @retval false if cold fix interval has not expired.
 */
static bool prv_check_cold_fix_hour_interval(void)
{
	/* Check if cold fix hour interval is set */
	if (Main_settings.ublox_cold_fix_hour_interval->def_val > 0) {
		/* Check if we are in the cold fix hour interval */
		if ((k_uptime_get() - prv_last_gps_coldfix) / 3600000 >=
		    Main_settings.ublox_cold_fix_hour_interval->def_val) {
			return true;
		}
	}
	return false;
}

/* PUBLIC FUNCTIONS */

int gps_init(void)
{
	LOG_INF("Start GPS Ublox init");

	prv_ctx.enabled = false;
	int err;

	/* Init power and vback pin */
	err = gps_init_gpio();
	if (err) {
		LOG_ERR("GPS init error: %d", err);
		return err;
	}

	k_msleep(1);

	/* Init communication device */
	err = gps_init_device();
	if (err) {
		LOG_ERR("Failed to init communication port for GPS!");
		return err;
	}

	LOG_INF("Waiting for GPS power on...");
	k_msleep(1000);

	LOG_INF("Enable NMEA messages.");
	gps_ublox_process_nmea_sentences();

	/* Start GPS */
	if (prv_ctx.com == I2C_DEV) {
		LOG_INF("Begin I2C");
		err = gps_ublox_begin_i2c(gps_dev);
		if (err) {
			/* Disable I2C
			 * gpio_pin_configure_dt(gpio_dev_pow, 12, GPIO_DISCONNECTED); EvaTODO
			 * gpio_pin_configure(gpio_dev_pow, 13, GPIO_DISCONNECTED); */
			LOG_ERR("Failed to begin i2c port! Err: %d", err);
		}
	} else {
		err = gps_ublox_begin_serial(gps_dev);
		if (err) {
			LOG_ERR("Failed to begin serial port! Err: %d", err);
		}
	}

	if (err) {
		LOG_ERR("Ublox GPS init error!");
		return err;
	}

	prv_ctx.enabled = true;
	prv_ctx.active_tracking = false;
	sys_err.ublox = 0;

	/* Set intervals */
	prv_ctx.default_send_interval = Main_settings.ublox_send_interval->def_val;
	prv_ctx.send_interval = prv_ctx.default_send_interval;
	prv_ctx.interval_type = 1;
	prv_ctx.skipped_attempts = 0;

	return 0;
}

int gps_reset(void)
{
	LOG_WRN("Reset GPS module!");

#if DT_NODE_EXISTS(GPS_VBCK_NODE)
	/* Power off */
	gpio_pin_set_dt(&gpio_dev_vbck, 0);
	sys_err.ublox = -EIO;
#else
	LOG_ERR("VBCK pin not defined in DT");
#endif

	gps_power(1);
	gps_ublox_reset();
	k_sleep(K_MSEC(2000));
	gps_power(0);

	/* int err = gps_init(); */
	sys_err.ublox = 0;
	prv_ctx.cold_fix = false;
	prv_ctx.hot_fix = false;
	prv_ctx.cold_retry = 0;
	prv_ctx.hot_retry = 0;
	prv_ctx.skipped_attempts = 0;

	return 0;
}

void gps_stop(void)
{
	/* Power off */
	gps_power(0);
	prv_ctx.enabled = false;

#ifdef CONFIG_LOW_POWER
	/* Disable UART or I2C */
	LOG_INF("Disable GPS com.\n");
	pm_device_action_run(gps_dev, PM_DEVICE_ACTION_TURN_OFF);
#endif

#if DT_NODE_EXISTS(GPS_VBCK_NODE)
	/* Power off */
	gpio_pin_set_dt(&gpio_dev_vbck, 0);
	sys_err.ublox = -EIO;
#else
	LOG_ERR("VBCK pin not defined in DT");
#endif
}

void gps_power(uint8_t state)
{
	/* Enable communication port */
	if (state) {
		/* Enable Ublox */
		gps_com_enable();
	}
#if DT_NODE_EXISTS(GPS_PWR_NODE)
	LOG_INF("Set Ublox GPS power: %d", state);
	if (state) {
		sys_err.ublox_busy = 1;
		gpio_pin_set_dt(&gpio_dev_pow, 1); /* GPS power */
		gps_ublox_reset_sat_data();        /* Reset satellite data */
		prv_ctx.fix_start = k_uptime_get();
		k_sleep(K_MSEC(500));
	} else {
		gpio_pin_set_dt(&gpio_dev_pow, 0);
		sys_err.ublox_busy = 0;
	}
#else
	LOG_WRN("GPS POWER pin not defined in DT!");
#endif

	/* Disable UART if applicable */
	if (!state) {
		/* Disable UART or I2C */
		gps_com_disable();
	}
}

bool gps_get_enabled(void)
{
	return prv_ctx.enabled;
}

/*!
 * @brief Get new gps data based on hot/cold fix status.
 *
 * @param[in] uint8_t *position  pointer to position message coordinates.
 * @param[in] uint8_t *status  pointer to status message, consisting of status[0] - success/fail,
 * status[1] - hot fix retries, status[2] - cold fix retries status[3] - timeToFix.
 *
 * @return integer error
 */
int gps_get_fix(void)
{
	/* Reset fix success */
	prv_ctx.last_fix_success = false;
	/* Reset time of last check for minimum number of satellites */
	prv_time_of_last_check_min_number_of_satellites = k_uptime_get();

	/* Check if gps is enabled */
	if (prv_ctx.enabled) {
		if (prv_ctx.active_tracking) {
			/* Clear old data */
			gps_ublox_flush();
			k_sleep(K_MSEC(RETRY_INTERVAL));

			prv_ctx.fix_start = k_uptime_get();
			sys_err.ublox_busy = 1;
			while ((uint32_t)((k_uptime_get() - prv_ctx.fix_start) / 1000) <
			       Main_settings.hot_fix_timeout->def_val) {
				if (!gps_get_new_data()) {
					prv_ctx.last_fix_success = true;
					break;
				}
				/* Periodically check if we have enough
				 * satellites for fix */
				if (prv_min_number_of_satellites_checker() == -EIO) {
					break;
				}
				k_sleep(K_MSEC(RETRY_INTERVAL));
			}
			/* Update ttf */
			prv_ctx.ttf = 0;
			sys_err.ublox_busy = 0;
		} else {
			gps_power(true);

			/* If cold fix was already successful and cold fix hour interval has expired
			 * perform new cold fix */
			if (prv_ctx.cold_fix && prv_check_cold_fix_hour_interval()) {
				prv_ctx.cold_fix = false;
				LOG_INF("Cold fix interval expired, performing new cold fix.");
			}

			/* Cold fix retry */
			if (!prv_ctx.cold_fix) {

				/* Clear buffer and flush data */
				gps_ublox_flush();
				k_sleep(K_MSEC(RETRY_INTERVAL));

				LOG_INF("Start cold fix acquisition, "
					"retry: %d of %d",
					prv_ctx.cold_retry, Main_settings.cold_fix_retry->def_val);
				while (!prv_ctx.last_fix_success &&
				       (uint32_t)((k_uptime_get() - prv_ctx.fix_start) / 1000) <
					       Main_settings.cold_fix_timeout->def_val) {
					if (!gps_get_new_data()) {
						prv_ctx.cold_fix = true;
						prv_ctx.cold_retry = 0;
						prv_ctx.hot_retry = 0;
						prv_ctx.last_fix_success = true;
						LOG_INF("We did get fix!");

						/* Update last cold fix time */
						prv_last_gps_coldfix = k_uptime_get();
						break;
					} else {
						k_sleep(K_MSEC(RETRY_INTERVAL));
					}
					/* Periodically check if we have
					 * enough satellites for fix
					 */
					if (prv_min_number_of_satellites_checker() == -EIO) {
						break;
					}
				}
				/* Update ttf */
				prv_ctx.ttf =
					(uint16_t)((k_uptime_get() - prv_ctx.fix_start) / 1000);
				LOG_INF("We did calculate ttf: %d", prv_ctx.ttf);

				/* If cold fix was obtained, leave ublox
				 * turned on and try to get better position
				 * for as long as cold fix was being
				 * obtained */
				if (prv_ctx.last_fix_success) {
					LOG_INF("Got valid cold fix in %d "
						"sec. Leave Ublox on for "
						"another 15 seconds.",
						prv_ctx.ttf);
					prv_ctx.fix_start = k_uptime_get();
					while ((uint32_t)((k_uptime_get() - prv_ctx.fix_start) /
							  1000) <
					       Main_settings.ublox_leave_on->def_val) {
						k_sleep(K_MSEC(RETRY_INTERVAL));
						gps_get_new_data();
					}
				} else {
					LOG_INF("Did not get cold fix in "
						"%d sec.",
						prv_ctx.ttf);
				}
				gps_power(false);
				if (!prv_ctx.cold_fix) {
					prv_ctx.cold_retry += 1; /* Increase retry count */
					/* If backoff, update send period */
					if (Main_settings.gps_backoff_factor->def_val < 10) {
						Main_settings.gps_backoff_factor->def_val = 10;
					}
					prv_ctx.send_interval =
						(uint32_t)(((float)Main_settings.gps_backoff_factor
								    ->def_val /
							    10) *
							   prv_ctx.send_interval); /* Increase */
					/* backoff */
					if (prv_ctx.send_interval >
					    Main_settings.ublox_send_interval->max) {
						prv_ctx.send_interval =
							Main_settings.ublox_send_interval->max;
					}
					LOG_INF("UBLOX send interval "
						"updated to: %d",
						prv_ctx.send_interval);
				}
				if (prv_ctx.cold_retry == Main_settings.cold_fix_retry->def_val) {
					prv_ctx.enabled = false;
					sys_err.ublox = -EIO;
					/* Turn off GPS */
				}
			} else {
				/* Clear buffer and flush data */
				gps_ublox_flush();
				k_sleep(K_MSEC(RETRY_INTERVAL));

				LOG_INF("Start hot fix acquisition, retry: "
					"%d of %d",
					prv_ctx.hot_retry, Main_settings.hot_fix_retry->def_val);
				prv_ctx.hot_fix = false;
				while ((uint32_t)((k_uptime_get() - prv_ctx.fix_start) / 1000) <
				       Main_settings.hot_fix_timeout->def_val) {
					LOG_INF("Hot fix: %d time",
						(uint32_t)((k_uptime_get() - prv_ctx.fix_start) /
							   1000));
					if (!gps_get_new_data()) {
						prv_ctx.hot_fix = true;
						prv_ctx.hot_retry = 0;
						prv_ctx.last_fix_success = true;
						if ((uint32_t)((k_uptime_get() -
								prv_ctx.fix_start) /
							       1000) >=
						    Main_settings.ublox_min_fix_time->def_val) {
							break;
						}
					}
					/* Periodically check if we have
					 * enough satellites for fix
					 */
					if (prv_min_number_of_satellites_checker() == -EIO) {
						break;
					}
					k_sleep(K_MSEC(RETRY_INTERVAL));
				}
				/* Update ttf */
				LOG_INF("Hot fix: %d time",
					(uint32_t)((k_uptime_get() - prv_ctx.fix_start) / 1000));
				prv_ctx.ttf =
					(uint16_t)((k_uptime_get() - prv_ctx.fix_start) / 1000);
				gps_power(false);
				if (!prv_ctx.hot_fix) {
					prv_ctx.hot_retry += 1; /* Increase retry count */
				}
				if (prv_ctx.hot_retry == Main_settings.hot_fix_retry->def_val) {
					prv_ctx.cold_fix = false;
				}
			}
#if DT_NODE_EXISTS(GPS_VBCK_NODE)
			gpio_pin_set_dt(&gpio_dev_vbck, (int)prv_ctx.cold_fix);
			LOG_INF("Set VBCK pin to: %d", (int)prv_ctx.cold_fix);
#endif
		}
	} else {
		return -EIO;
	}

	/* Update ublox fix status */
	if (prv_ctx.last_fix_success) {
		prv_ctx.send_interval = prv_ctx.default_send_interval;
		sys_err.ublox_fix = 0;
	} else {
		sys_err.ublox_fix = -EIO;
	}
	return 0;
}

bool gps_get_last_fix_success(void)
{
	return prv_ctx.last_fix_success;
}

void gps_get_last_fix_status(bool *fix, uint8_t *hot_retry, uint8_t *cold_retry, uint16_t *ttf)
{
	*fix = prv_ctx.last_fix_success;
	*hot_retry = prv_ctx.hot_retry;
	*cold_retry = prv_ctx.cold_retry;
	*ttf = prv_ctx.ttf;
}

void gps_get_last_fix_data(struct gps_ublox_position_data *position)
{
	*position = prv_ctx.position;
}

void gps_get_last_fix_time(uint32_t *time)
{
	*time = prv_ctx.fix_time;
}

int gps_get_sat_data(uint8_t *payload, uint8_t max_payload)
{
	if (prv_ctx.enabled) {
		return gps_ublox_get_sat_data(payload, max_payload);
	} else {

		return -EIO;
	}
}

bool gps_get_active_tracking(void)
{
	return prv_ctx.active_tracking;
}

bool gps_send_interval(void)
{
	/* Check if GPS enabled */
	if (!prv_ctx.enabled) {
		return false;
	} else {
		/* Motion detection can run even if GPS intervals are disabled. */
		/* Check if minimum in-between-fix time elapsed */
		if (((uint32_t)((k_uptime_get() - prv_ctx.last_fix_try) / 1000) >
		     FIX_MINIMUM_INTERVAL)) {
			/* Check if there were enough motion trigger events per user
			 * defined duration to start new fix */
			if (prv_gps_check_motion_detection_counter()) {
				return true;
			}
		}
	}

	/* Check active tracking status */
	gps_check_active_tracking();

	/* Check interval type */
	uint8_t old_interval_type = prv_ctx.interval_type;
	prv_ctx.interval_type = gps_check_time_interval_type();
	/* Check if change */
	if (old_interval_type != prv_ctx.interval_type) {
		prv_ctx.cold_retry = 0;
		prv_ctx.cold_fix = false;
		prv_ctx.hot_retry = 0;
		prv_ctx.hot_fix = false;
	}

	/* Check for settings update */
	gps_update_send_interval(prv_ctx.interval_type);
	/* Check if interval is not defined */
	if (prv_ctx.send_interval == 0) {
		return false;
	}

	/* First check - Wait FIX_MINIMUM_INTERVAL before conducting first fix */
	if (prv_ctx.last_fix_try == 0 && k_uptime_get() / 1000 > FIX_MINIMUM_INTERVAL) {
		LOG_INF("First position attempt.");
		return true;
	}

	/* Check if minimum in-between-fix time elapsed */
	if (((uint32_t)((k_uptime_get() - prv_ctx.last_fix_try) / 1000) > FIX_MINIMUM_INTERVAL)) {
		/* Check if there were enough motion trigger events per user
		 * defined duration to start new fix */
		if (prv_gps_check_motion_detection_counter()) {
			return true;
		}
	}

	/* Periodic check */
	if ((uint32_t)((k_uptime_get() - prv_ctx.last_fix_try) / 1000) > prv_ctx.send_interval) {
		/* Check if motion triggered gps functionality is supported
		 */
		if (Main_settings.enable_motion_trig_gps->def_val && !sys_err.acc) {
			/* Check which GPS motion detection mode we are
			 * using */
			if (Main_settings.gps_motion_triggered_min_num_of_triggers_per_interval
					    ->def_val != 0 &&
			    Main_settings.gps_triggered_interval->def_val != 0) {
				return false;
			}

			/* Check if motion triggered GPS needs to be
			disabled due to board type or accelerometer error */
#ifndef CONFIG_IRNAS_LIS2DW12_MOTION_DETECTION
			Main_settings.enable_motion_trig_gps->def_val = false;
			prv_ctx.skipped_attempts = 0;
#endif
			/* Check if skipped attempts reached maximal number
			 */
			if (prv_ctx.skipped_attempts + 1 >=
			    Main_settings.gps_skipped_triggered_interval->def_val) {
				LOG_INF("Skipped attempts: %d, obtain "
					"position!",
					prv_ctx.skipped_attempts);
				prv_ctx.skipped_attempts = 0;
				return true;
			}
			/* Check if motion was detected recently */
			if (k_uptime_get() - Main_values.last_accel_int_time->def_val <
			    prv_ctx.send_interval) {
				LOG_INF("Last motion trigger: %d s ago. "
					"Get position!",
					(uint32_t)(get_global_unix_time() -
						   Main_values.last_accel_int_time->def_val) /
						1000);
				prv_ctx.skipped_attempts = 0;
				return true;
			} else {
				LOG_INF("Last motion trigger: %d s ago. "
					"Skip fix!",
					(uint32_t)(k_uptime_get() -
						   Main_values.last_accel_int_time->def_val) /
						1000);
				prv_ctx.skipped_attempts++;
				prv_ctx.last_fix_try = k_uptime_get();
				return false;
			}
		}

		LOG_INF("GPS interval elapsed.");
		return true;
	}

	return false;
}

void gps_motion_triggered_event_handler(void)
{
	/* Check if enabled */
	if (Main_settings.enable_motion_trig_gps->def_val == false ||
	    Main_settings.gps_motion_triggered_min_num_of_triggers_per_interval->def_val == 0 ||
	    Main_settings.gps_triggered_interval->def_val == 0) {
		return;
	}

	prv_gps_motion_event_timestamps[prv_gps_motion_event_timestamp_idx] =
		(uint32_t)k_uptime_get();

	/* Check number of events */
	/* i will be the index of the left-most timestamp within the
	 * gps_motion_triggered_min_num_of_triggers_per_interval of the latest timestamp */
	int i;
	int num_events = 0;
	for (i = prv_gps_motion_event_timestamp_idx; i >= 0; i--) {
		if (((uint32_t)k_uptime_get() - prv_gps_motion_event_timestamps[i]) / 1000 >
		    Main_settings.gps_triggered_interval->def_val) {
			break;
		}
		num_events++;
	}

	/* Shift array, overwriting old events */
	/* i + 1 is the left-most VALID timestamp */
	memmove(&prv_gps_motion_event_timestamps[0], &prv_gps_motion_event_timestamps[i + 1],
		sizeof(uint32_t) * (num_events));

	/* Update iterator */
	prv_gps_motion_event_timestamp_idx = num_events;

	LOG_INF("Motion trigger event detected. Event counter: %u/%u (interval: %us)",
		prv_gps_motion_event_timestamp_idx,
		Main_settings.gps_motion_triggered_min_num_of_triggers_per_interval->def_val,
		Main_settings.gps_triggered_interval->def_val);

	if (num_events >=
	    Main_settings.gps_motion_triggered_min_num_of_triggers_per_interval->def_val) {
		prv_gps_motion_get_fix = true;
		prv_gps_motion_detection_clear_all_timestamps();
	}
}

void gps_update_last_fix_time(void)
{
	prv_ctx.last_fix_try = k_uptime_get();
}
