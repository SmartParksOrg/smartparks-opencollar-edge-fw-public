/** @file lis2dw12_sensors.c
 *
 * @brief Interface to sensor drives, accelerometer, GPS
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <math.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "generated_settings.h"
#include "global_time.h"
#include "gps.h"
#include "lis2dw12_sensor_fifo.h"
#include <irnas_lis2dw12_types.h>

#include "lis2dw12_sensor.h"

#define LIS2_ACCEL_TIMEOUT 200

LOG_MODULE_REGISTER(LIS2DW12_SENSOR, 3);

#define LIS2DW12_SENSOR_WATERMARK_STARTUP_DELAY_MS 25000

/* Accelerometer mutex */
K_MUTEX_DEFINE(lis2dw12_mutex);

/* Devices */
#if DT_NODE_EXISTS(DT_NODELABEL(lis2dw12_accel))
const struct device *prv_lis2dw12_sensor = DEVICE_DT_GET(DT_NODELABEL(lis2dw12_accel));
#else
const struct device *lis2dw12_sensor;
#endif /* Devices */

#ifdef CONFIG_IRNAS_LIS2DW12_FIFO
/* Public variables */
bool lis2dw12_sensor_fifo_enabled = false;

/* Private variables */

/* We currently can not control the amount of samples we want to read from the FIFO buffer. All
 * available samples get read and saved into the buffer when read. If we have less space available
 * in the buffer than there are samples in the FIFO we could get buffer overflow. Therefore we
 * extended the buffer for the maximum amount of possible FIFO samples (LIS2DW12_FIFO_MAX_SAMPLES).
 *
 * TLDR: We add LIS2DW12_FIFO_MAX_SAMPLES (32) samples for each axis to avoid potential overflow.
 */
static struct sensor_value
	prv_accel_data_buf[(NUMBER_OF_SAMPLES_PER_MOTION_EVENT + LIS2DW12_FIFO_MAX_SAMPLES) * 3];
static int prv_accel_data_buf_itr = 0;
#endif /* CONFIG_IRNAS_LIS2DW12_FIFO */

uint8_t motion_ths_setting;

uint16_t prv_lis2dw12_odr_value = 0;
uint8_t prv_lis2dw12_g_scale = 0;

#ifdef CONFIG_IRNAS_LIS2DW12_TRIGGER

#ifdef CONFIG_IRNAS_LIS2DW12_FIFO
/**
 * @brief Print the buffer with accelerometer data.
 *
 * @param buf Buffer with accelerometer data.
 */
static void prv_print_buff(struct sensor_value *buf)
{
	LOG_INF("Accelerometer data: ");
	for (int i = 0; i < NUMBER_OF_SAMPLES_PER_MOTION_EVENT * 3; i += 3) {
		/* We use printk here for increased readability */
		if (buf[i].val2 < 0) {
			buf[i].val2 *= -1;
		}
		if (buf[i + 1].val2 < 0) {
			buf[i + 1].val2 *= -1;
		}
		if (buf[i + 2].val2 < 0) {
			buf[i + 2].val2 *= -1;
		}
		printk("x: %d.%04d; y: %d.%04d; z: %d.%04d\n", buf[i].val1, buf[i].val2,
		       buf[i + 1].val1, buf[i + 1].val2, buf[i + 2].val1, buf[i + 2].val2);

		k_sleep(K_MSEC(5)); /* Sleep so we don't drop logs */
	}
}
#endif /* CONFIG_IRNAS_LIS2DW12_FIFO */

/**
 * @brief Handler for LIS2DW12 sensor trigger.
 *
 * @param dev Device structure for the LIS2DW12 sensor.
 * @param trig Trigger structure containing the type of trigger.
 */
static void prv_lis2dw12_sensor_trigger_handler(const struct device *dev,
						const struct sensor_trigger *trig)
{
	Main_values.last_accel_int_time->def_val = get_global_unix_time();
	LOG_INF("LIS2DW12 interrupt at: %d", Main_values.last_accel_int_time->def_val);

	/* Check if event originates from motion detection */
	if (trig->type == SENSOR_TRIG_DELTA) {
		gps_motion_triggered_event_handler();

#ifdef CONFIG_IRNAS_LIS2DW12_FIFO
		if (lis2dw12_sensor_fifo_enabled) {

			/* Start with accelerometer in bypass */
			if (k_uptime_get() < LIS2DW12_SENSOR_WATERMARK_STARTUP_DELAY_MS) {
				lis2dw12_sensor_set_fifo_mode(LIS2DW12_BYPASS_MODE);
			} else {
				lis2dw12_sensor_fifo_check_mode();
			}
		}
#endif /* CONFIG_IRNAS_LIS2DW12_FIFO */
	}
#ifdef CONFIG_IRNAS_LIS2DW12_FIFO
	if (trig->type == (int)SENSOR_TRIG_IRNAS_FIFO_WATERMARK &&
	    k_uptime_get() > LIS2DW12_SENSOR_WATERMARK_STARTUP_DELAY_MS) {

		/* Read samples into buffer */
		int read_samples = lis2dw12_sensor_fifo_fetch(
			prv_lis2dw12_sensor, &prv_accel_data_buf[prv_accel_data_buf_itr],
			ARRAY_SIZE(prv_accel_data_buf) - prv_accel_data_buf_itr);

		if (read_samples < 0) {
			LOG_ERR("Failed to read samples from FIFO: %d", read_samples);
		} else {
			prv_accel_data_buf_itr += read_samples;
		}

		/* Check if enough samples were read */
		if (prv_accel_data_buf_itr >= NUMBER_OF_SAMPLES_PER_MOTION_EVENT * 3) {
			/* Print accelerometer data */
			prv_print_buff(prv_accel_data_buf);

			prv_accel_data_buf_itr = 0;

			/* Set to bypass mode to reset FIFO buffer */
			lis2dw12_sensor_set_fifo_mode(LIS2DW12_BYPASS_MODE);
			/* Set FIFO mode to bypass-to-continuous mode */
			lis2dw12_sensor_set_fifo_mode(LIS2DW12_BYPASS_TO_STREAM_MODE);
		}
	}
#endif /* CONFIG_IRNAS_LIS2DW12_FIFO */
}

/**
 * @brief Configure motion threshold.
 *
 * @param[in] ths threshold
 * @return int 0 or negative error code
 */
static int lis2dw12_sensor_cfg_motion_ths(uint8_t ths)
{
	if (!prv_lis2dw12_sensor) {
		LOG_ERR("lis2dw12 init error!");
		return -EIO;
	}

	struct sensor_value ths_attr;
	ths_attr.val1 = ths;
	ths_attr.val2 = 0;

	int err = sensor_attr_set(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_TH,
				  &ths_attr);
	if (err < 0) {
		LOG_ERR("Cannot set threshold for LIS2DW12 accel");
	}

	return err;
}

/**
 * @brief Configure motion threshold duration.
 *
 * @param[in] dur - duration
 * @return int 0 or negative error code
 */
__attribute__((unused)) static int lis2dw12_sensor_cfg_motion_dur(uint8_t dur)
{
	if (!prv_lis2dw12_sensor) {
		LOG_ERR("lis2dw12 init error!");
		return -EIO;
	}

	struct sensor_value dur_attr;
	dur_attr.val1 = dur;
	dur_attr.val2 = 0;

	int err = sensor_attr_set(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_DUR,
				  &dur_attr);
	if (err < 0) {
		LOG_ERR("Cannot set duration for LIS2DW12 accel");
	}

	return err;
}

/**
 * @brief Enable or disable sensor interrupt.
 *
 * @param[in] enable - enable/disable
 * @return int 0 or negative error code
 */
__attribute__((unused)) static int lis2dw12_sensor_cfg_interrupts(bool enable)
{
	if (!prv_lis2dw12_sensor) {
		LOG_ERR("lis2dw12 init error!");
		return -EIO;
	}

	struct sensor_value en_attr;
	en_attr.val1 = (int)enable;
	en_attr.val2 = 0;

	int err = sensor_attr_set(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_ALERT,
				  &en_attr);
	if (err < 0) {
		LOG_ERR("Cannot enable/disable int for LIS2DW12 accel");
	}

	return err;
}

/**
 * @brief Configure trigger.
 *
 * @return int 0 or negative error code
 */
static int lis2dw12_sensor_trigger_setup(void)
{
	if (!prv_lis2dw12_sensor) {
		LOG_ERR("lis2dw12 init error!");
		return -ENODEV;
	}

#if CONFIG_IRNAS_LIS2DW12_TAP
	struct sensor_trigger trig_tap;
	trig_tap.type = SENSOR_TRIG_TAP;
	if (sensor_trigger_set(lis2dw12_sensor, &trig_tap, prv_lis2dw12_sensor_trigger_handler)) {
		LOG_ERR("Failed to set tap trigger!");
		return -EIO;
	}
#endif /* CONFIG_IRNAS_LIS2DW12_TAP */

#if CONFIG_IRNAS_LIS2DW12_FREE_FALL
	/* lis2dw12_sensor_cfg_odr(100); */
	struct sensor_trigger trig_ff;
	trig_ff.type = SENSOR_TRIG_FREEFALL;
	if (sensor_trigger_set(lis2dw12_sensor, &trig_ff, prv_lis2dw12_sensor_trigger_handler)) {
		LOG_ERR("Failed to set free fall trigger!");
		return -EIO;
	}
#endif /* CONFIG_IRNAS_LIS2DW12_FREE_FALL */

#if CONFIG_IRNAS_LIS2DW12_MOTION_DETECTION
	/* lis2dw12_sensor_cfg_odr(12); */
	struct sensor_trigger trig_motion;
	trig_motion.type = SENSOR_TRIG_DELTA;
	if (sensor_trigger_set(prv_lis2dw12_sensor, &trig_motion,
			       prv_lis2dw12_sensor_trigger_handler)) {
		LOG_ERR("Failed to set motion trigger!");
		return -EIO;
	}
	/* Change ths */
	lis2dw12_sensor_cfg_motion_ths(Main_settings.motion_ths->def_val);
	motion_ths_setting = Main_settings.motion_ths->def_val;
#endif /* CONFIG_IRNAS_LIS2DW12_MOTION_DETECTION */

#if CONFIG_IRNAS_LIS2DW12_FIFO
	if (Main_settings.accel_movement_data_fifo_enabled->def_val) {
		int err = lis2dw12_sensor_fifo_check_mode();
		if (err) {
			LOG_ERR("Failed to check FIFO mode: %d", err);
			return err;
		}

		err = lis2dw12_fifo_watermark_trigger_set(prv_lis2dw12_sensor,
							  prv_lis2dw12_sensor_trigger_handler);
		if (err < 0) {
			LOG_ERR("Failed to set FIFO trigger!");
			return err;
		}

		lis2dw12_sensor_fifo_enabled = true;
		LOG_DBG("FIFO mode set and watermark trigger enabled!");

		err = lis2dw12_sensor_fifo_check_mode();
	}
#endif /* CONFIG_IRNAS_LIS2DW12_FIFO */
	return 0;
}

#endif /* CONFIG_IRNAS_LIS2DW12_TRIGGER */

/**
 * @brief Set g scale for LIS2DW12 accelerometer.
 *
 * @param[in] g_scale - g scale mode (accepted values: 2g, 4g, 8g, 16g)
 * @return int 0 or negative error code
 */
static int lis2dw12_sensor_set_g_scale(int g_scale)
{
	if (!prv_lis2dw12_sensor) {
		LOG_ERR("lis2dw12 init error!");
		return -EIO;
	}

	if (g_scale != 2 && g_scale != 4 && g_scale != 8 && g_scale != 16) {
		LOG_ERR("Invalid G scale value: %d", g_scale);
		return -EINVAL; /* Invalid argument */
	}

	struct sensor_value g_scale_attr;
	/* Convert to m/s² because the driver expects this data type */
	sensor_g_to_ms2(g_scale, &g_scale_attr);

	int err = sensor_attr_set(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ,
				  SENSOR_ATTR_FULL_SCALE, &g_scale_attr);
	if (err < 0) {
		LOG_ERR("Cannot set G scale for LIS2DW12 accel");
	}

	return err;
}

/**
 * @brief Configure odr setting.
 *
 * @param[in] odr
 * @return int 0 or negative error code
 */
__attribute__((unused)) static int lis2dw12_sensor_cfg_odr(int odr)
{
	if (!prv_lis2dw12_sensor) {
		LOG_ERR("lis2dw12 init error!");
		return -EIO;
	}

	struct sensor_value odr_attr;
	odr_attr.val1 = odr;
	odr_attr.val2 = 0;

	int err = sensor_attr_set(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ,
				  SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
	if (err < 0) {
		LOG_ERR("Cannot set sampling frequency for LIS2DW12 accel");
	}

	return err;
}

int lis2dw12_sensor_init(void)
{
	if (!prv_lis2dw12_sensor) {
		LOG_ERR("DT binding for lis2dw12 not provided!");
		return -ENODEV;
	} else {
		LOG_INF("Bind to LIS2DW12.");
	}
	if (!device_is_ready(prv_lis2dw12_sensor)) {
		LOG_ERR("Device %s is not ready", prv_lis2dw12_sensor->name);
	}

#if CONFIG_IRNAS_LIS2DW12_TRIGGER
	lis2dw12_sensor_trigger_setup();
#endif

	/* Set ODR */
	prv_lis2dw12_odr_value = Main_settings.accel_odr_hz->def_val;
	lis2dw12_sensor_cfg_odr(Main_settings.accel_odr_hz->def_val);

	/* Set g_scale */
	prv_lis2dw12_g_scale = Main_settings.accel_g_scale->def_val;
	lis2dw12_sensor_set_g_scale(prv_lis2dw12_g_scale);

	return 0;
}

int lis2dw12_sensor_get_data(struct sensor_value *val)
{
	if (!prv_lis2dw12_sensor) {
		LOG_ERR("LIS2DW12 not available!");
		return -EIO;
	}
	int err = sensor_sample_fetch(prv_lis2dw12_sensor);
	if (!err) {
		err = sensor_channel_get(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ, val);
	}

	if (err < 0) {
		LOG_ERR("LIS2DW12 Update failed: %d", err);
	} else {
		LOG_INF("x: %d.%04d; y: %d.%04d; z: %d.%04d\n", val[0].val1, val[0].val2,
			val[1].val1, val[1].val2, val[2].val1, val[2].val2);
	}
	return err;
}

int lis2dw12_sensor_update_settings(void)
{
	int err = 0;
#if CONFIG_IRNAS_LIS2DW12_MOTION_DETECTION
	if (motion_ths_setting != Main_settings.motion_ths->def_val) {
		motion_ths_setting = Main_settings.motion_ths->def_val;
		err = lis2dw12_sensor_cfg_motion_ths(motion_ths_setting);
		if (err < 0) {
			LOG_ERR("Failed to set motion ths: %d", err);
		}
	}
#endif
#ifdef CONFIG_IRNAS_LIS2DW12_FIFO
	if (lis2dw12_sensor_fifo_enabled !=
	    Main_settings.accel_movement_data_fifo_enabled->def_val) {
		lis2dw12_sensor_fifo_enabled =
			Main_settings.accel_movement_data_fifo_enabled->def_val;
		if (lis2dw12_sensor_fifo_enabled) {
			/* Set trigger */
			lis2dw12_fifo_watermark_trigger_set(prv_lis2dw12_sensor,
							    prv_lis2dw12_sensor_trigger_handler);

			/* Check if FIFO is in bypass-to-continuous mode */
			err = lis2dw12_sensor_fifo_check_mode();
			if (err) {
				LOG_ERR("Failed to check FIFO mode: %d", err);
				return err;
			}
		} else {
			/* Unset trigger */
			lis2dw12_fifo_watermark_trigger_set(prv_lis2dw12_sensor, NULL);
		}
	}
#endif /* CONFIG_IRNAS_LIS2DW12_FIFO */

	/* Configure ODR if changed */
	if (prv_lis2dw12_odr_value != Main_settings.accel_odr_hz->def_val) {
		prv_lis2dw12_odr_value = Main_settings.accel_odr_hz->def_val;
		err = lis2dw12_sensor_cfg_odr(prv_lis2dw12_odr_value);
		if (err < 0) {
			LOG_ERR("Failed to set ODR: %d. Error: %d", prv_lis2dw12_odr_value, err);
			return err;
		} else {
			LOG_INF("Set ODR to %d", prv_lis2dw12_odr_value);
		}
	}

	if (prv_lis2dw12_g_scale != Main_settings.accel_g_scale->def_val) {
		prv_lis2dw12_g_scale = Main_settings.accel_g_scale->def_val;

		err = lis2dw12_sensor_set_g_scale(prv_lis2dw12_g_scale);
		if (err < 0) {
			LOG_ERR("Failed to set G scale: %d", err);
			return err;
		} else {
			LOG_INF("Set G scale mode to %d", prv_lis2dw12_g_scale);
		}
	}

	return err;
}
