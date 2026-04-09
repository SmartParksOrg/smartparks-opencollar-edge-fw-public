/** @file fence.c
 *
 * @brief Interface for fence module.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#include <fence_port_common.h>
#include <led.h>
#include <settings_def.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <fence.h>

#define FENCE_NOISE_THRESHOLD_V   0  // Threshold for cutout noise EvaTODO
#define FENCE_NOISE_THRESHOLD_RAW 75 // Threshold for cutout noise EvaTODO

// Number of samples needed to determine pulse/no pulse state of the pulse
#define FENCE_STATE_N_SAMPLES  50
#define FENCE_START_TIMEOUT_MS 1000 // Timeout, waiting for "no peek" part of sample

#define FENCE_PULSES_TO_DETECT 5 // Number of pulses we want to detect in sampling
#define FENCE_MV_IN_V          1000
#define FENCE_DEFAULT_SCALING  100000

LOG_MODULE_REGISTER(fence, 3);

/* Measurement status */
enum fence_data_status {
	FENCE_MEASUREMENT_SUCCESSFUL = 0,
	FENCE_ERR_POWER = 1,
	FENCE_ERR_NO_PULSE = 2,
	FENCE_ERR_ADC = 3,
	FENCE_ERR_DEFAULT = 4
} __attribute__((packed));

/**
 * @brief Pulse data structure.
 *
 */
struct fence_pulse {
	/* Success /fail */
	enum fence_data_status success;
	/* Detected pulse counter */
	uint8_t counter;
	/* Average pulse voltage in mV */
	uint16_t voltage;
	/* Pulse energy */
	uint16_t energy;
} __attribute__((packed));

enum fence_pulse_status {
	FENCE_NO_PULSE = 0,
	FENCE_PULSE = 1
};

static bool prv_fence_initialized = false;

/* Data of fence ADC io-channels */
static const struct adc_dt_spec fence_adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

/* Enable pin binding */
const struct gpio_dt_spec fence_en_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(fence_port_gpio), gpios);

/* Local scaling factor will result in 10kV output for 1000 mV measurement */
int32_t scaling_factor = FENCE_DEFAULT_SCALING;

/* Local buffer for sampling */
int16_t sample_raw;
static struct adc_sequence sequence = {
	.buffer = &sample_raw,
	.buffer_size = sizeof(sample_raw),
};

/**
 * @brief Enable ADC measurement.
 *
 * @return a value from gpio_pin_set_dt()
 */
static int fence_enable(void)
{
	return gpio_pin_set_dt(&fence_en_gpio, 1);
}

/**
 * @brief Disable ADC measurement.
 *
 * @return a value from gpio_pin_set_dt()
 */
static int fence_disable(void)
{
	return gpio_pin_set_dt(&fence_en_gpio, 0);
}

/**
 * @brief Configure ADC channel and initialize measurement sequence.
 *
 * @return 0 if ok
 * @return -ENODEV if ADC device is not responsive
 * @return A value from adc_channel_setup() or -ENOTSUP if information from
 * Devicetree is not valid.
 */
static int fence_adc_channel_configure(void)
{
	int err = 0;
	if (!device_is_ready(fence_adc_channel.dev)) {
		LOG_ERR("Fence ADC controller device not ready");
		return -ENODEV;
	}

	err = adc_channel_setup_dt(&fence_adc_channel);
	if (err) {
		LOG_ERR("Could not setup fence ADC channel.");
	}

	/* Init sequence */
	adc_sequence_init_dt(&fence_adc_channel, &sequence);

	return err;
}

/**
 * @brief Set local scaling factor in form factor * 10^5.
 *
 * @param[in] scaling scaling factor in form factor * 10^5.
 */
static void fence_set_scaling_factor(uint32_t scaling)
{
	scaling_factor = (int32_t)scaling;
}

/**
 * @brief Convert raw value to mV.
 *
 * @param raw raw value
 * @return uint16_t V value. If error, or value is below 0, 0 will be returned.
 */
static uint16_t fence_raw_to_v(int16_t raw)
{
	int32_t mv = raw;
	if (adc_raw_to_millivolts_dt(&fence_adc_channel, &mv)) {
		/* If error return 0 */
		return 0;
	}

	if (mv < 0) {
		return 0;
	}

	/* Apply scaling factor and convert mV to V*/
	mv = (mv * scaling_factor) / FENCE_MV_IN_V;

	return (uint16_t)mv;
}

/**
 * @brief Perform single sampling sequence.
 * Function will take local adc_dt_spec fence_adc_channel ADC dev and adc_sequence sequence, that
 * both should be configured before calling this function and performed sampling on given ADC
 * channel with configured settings. Raw value is stored in the sequence buffer, sample_raw local
 * variable.
 *
 * @return int adc_read() error.
 */
static int fence_sample(void)
{
	/* Sample */
	int err = adc_read(fence_adc_channel.dev, &sequence);

	/* For debug purposes only */
	/*
	int32_t val_mv = sample_raw;
	adc_raw_to_millivolts_dt(&fence_adc_channel, &val_mv);

	printk("%d %dmV\n", sample_raw, val_mv);
	k_msleep(1);
	*/

	return err;
}

/**
 * @brief Wait for part of the signal without pulse.
 * Wait for FENCE_STATE_N_SAMPLES in a row to be below FENCE_NOISE_THRESHOLD_RAW value to
 * label signal as no pulse. If we do not detect no pulse, return timeout.
 *
 * @retval 0 - signal without pulse found
 * @retval adc_read error
 * @retval -ETIME - timeout
 */
static int fence_wait_for_no_pulse(void)
{
	int err = 0;
	int counter = 0;
	int64_t start = k_uptime_get();

	/* Detect part of the signal without peek */
	while (k_uptime_get() - start < FENCE_START_TIMEOUT_MS) {
		err = fence_sample();
		if (err) {
			return err;
		}

		/* If raw value is less then no pulse threshold, count as no pulse */
		if (sample_raw <= FENCE_NOISE_THRESHOLD_RAW) {
			counter++;
		} else {
			counter = 0;
		}
		/* Check if we have enough samples without pulse */
		if (counter == FENCE_STATE_N_SAMPLES) {
			return 0;
		}
	}

	return -ETIME;
}

/**
 * @brief Pulse detection process.
 * Assume you are in "no pulse state" coming from fence_wait_for_no_pulse() function. Read next
 * sample, track you max value and add it to your sum. Asses if you need to change your state to
 * FENCE_NO_PULSE/FENCE_PULSE. Reset counters when switching states. If you are coming to
 * FENCE_NO_PULSE state from FENCE_PULSE, count pulse, update pulse peak value and sum. Reset all
 * variables and repeat process, until timeout defined by duration in seconds happens.
 * If you detected some pulses, calculate their average peak voltage and energy.
 *
 * @param[in] duration sampling duration in seconds
 * @param[out] pulse fence_pulse struct
 * @return int adc_read() error.
 */
static int fence_detect_pulses(uint16_t duration, struct fence_pulse *pulse)
{
	int err = 0;

	/* Local variables used for calculation */
	uint16_t val_v = 0;
	uint16_t peek_max = 0;
	uint32_t peek_average = 0;
	uint64_t peek_sum = 0;
	uint64_t sum_v = 0;

	/* Counters for samples labeled as pulse/no pulse */
	int pulse_sample_counter = 0;
	int no_pulse_sample_counter = 0;

	/* Assume we are in no pulse state, as we have come from fence_wait_for_no_pulse() function
	 */
	enum fence_pulse_status status = FENCE_NO_PULSE;

	/* Reset pulse counter */
	pulse->counter = 0;

	/* Convert duration to ms */
	int64_t duration_ms = (int64_t)duration * MSEC_PER_SEC;

	/* Start test */
	int64_t start = k_uptime_get();

	while (k_uptime_get() - start < duration_ms) {
		err = fence_sample();
		if (err) {
			return err;
			LOG_ERR("Error when sampling!");
		}

		/* Convert to scaled V */
		val_v = fence_raw_to_v(sample_raw);

		/* Lower threshold cutout noise in calculation */
		if (sample_raw < FENCE_NOISE_THRESHOLD_RAW) {
			val_v = 0;
		}

		/* Update energy sum -EvaTODO */
		peek_sum += val_v;
		/* Update highest recorded value for this peek */
		if (val_v > peek_max) {
			peek_max = val_v;
		}

		/* Decide if value is puse or no pule and modify counters accordingly */
		if (val_v > 0) {
			pulse_sample_counter++;
			no_pulse_sample_counter = 0;
		} else {
			pulse_sample_counter = 0;
			no_pulse_sample_counter++;
		}

		/* Switch state based on the counters */
		if (status == FENCE_NO_PULSE && pulse_sample_counter >= FENCE_STATE_N_SAMPLES) {
			status = FENCE_PULSE;
			if (Main_settings.fence_led_blink->def_val) {
				led_blink(1, LED_G);
			}
			LOG_INF("Pulse detected with: %d samples.", pulse_sample_counter);
		} else if (status == FENCE_PULSE &&
			   no_pulse_sample_counter >= FENCE_STATE_N_SAMPLES) {
			status = FENCE_NO_PULSE;
			LOG_INF("Pulse end detected with: %d samples, peak max: %d sum %lld.",
				no_pulse_sample_counter, peek_max, sum_v);
			/* New end of the pulse is detected */
			pulse->counter++;
			peek_average += peek_max;
			peek_max = 0;
			sum_v += peek_sum;
			peek_sum = 0;
		}

		/* If sufficient number of pulses is detected, exit - do not use this for now */
		/*
		if (pulse->counter == FENCE_PULSES_TO_DETECT) {
			break;
		}
		*/
	}

	/* Calculate average energy and voltage */
	if (pulse->counter > 0) {
		pulse->energy = (uint16_t)((sum_v / pulse->counter) / 100);
		pulse->voltage = (uint16_t)(peek_average / pulse->counter);
	} else {
		pulse->energy = 0;
		pulse->voltage = 0;
	}

	return 0;
}

int fence_init(void)
{

	int err = 0;

	if (prv_fence_initialized) {
		return -EALREADY;
	}

	/* Configure enable pin */
	err = gpio_pin_configure_dt(&fence_en_gpio, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Failed to configure fence enable pin");
		return err;
	}

	/* Configure ADC */
	err = fence_adc_channel_configure();
	if (err) {
		LOG_ERR("Failed to configure ADC channel");
		return err;
	}

	prv_fence_initialized = true;

	return 0;
}

int fence_deinit(void)
{
	if (prv_fence_initialized == false) {
		return -EALREADY;
	}
	prv_fence_initialized = false;
	return 0;
}

int fence_measure(uint16_t duration, uint32_t scaling, uint8_t *msg, uint8_t *msg_len)
{
	int err = 0;

	if (prv_fence_initialized == false) {
		LOG_ERR("Fence module not initialized!");
		err = fence_port_common_check_settings();
		if (err) {
			LOG_ERR("Failed to check fence settings: %d", err);
			return err;
		}
	}

	/* Measurement struct */
	struct fence_pulse pulse = {
		.success = FENCE_ERR_DEFAULT, .counter = 0, .voltage = 0, .energy = 0};

	/* Set scaling factor */
	LOG_INF("Set scaling factor: %d", scaling);
	fence_set_scaling_factor(scaling);

	/* Enable sampling */
	if (fence_enable()) {
		pulse.success = FENCE_ERR_POWER;
		return -EIO;
	}

	/* For debug only! */
	/*
	while (1) {
		err = fence_sample();
		if (err) {
			goto exit;
		}
		LOG_INF("R: %d", sample_raw);
		k_sleep(K_MSEC(10));
	}
	*/

	/* Wait for no-pulse part */
	if (fence_wait_for_no_pulse()) {
		LOG_ERR("Failed to identify no-pulse part of the ADC signal.");
		pulse.success = FENCE_ERR_NO_PULSE;
		goto exit;
	}
	LOG_INF("Got to no pulse part.");

	/* Perform measurement sequence */
	err = fence_detect_pulses(duration, &pulse);
	if (err) {
		LOG_ERR("Failed to perform measurement sequence the ADC signal.");
		pulse.success = FENCE_ERR_ADC;
		goto exit;
	}

	/* Set measurement as successful */
	pulse.success = FENCE_MEASUREMENT_SUCCESSFUL;

exit:
	/* Copy data to output message */
	memcpy(msg, &pulse, sizeof(pulse));
	*msg_len = sizeof(pulse);

	if (fence_disable()) {
		return -EIO;
	}

	return err;
}
