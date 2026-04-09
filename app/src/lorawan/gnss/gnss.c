/**
 * @file gnss.c
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#include "gnss.h"

#include "lr11xx_board.h"
#include "lr11xx_gnss.h"
#include "lr11xx_gnss_types.h"
#include "lr11xx_system.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(gnss);

#define IRQ_MASK       LR11XX_SYSTEM_IRQ_GNSS_SCAN_DONE
#define GNSS_MAX_SV    12
#define NAV_MAX_LENGTH 255
#define DATA_PER_SAT   2

/**
 * @brief Reference time structure
 *
 */
struct gnss_gps_ref_time {
	uint32_t gps_time;     /* GPS time in seconds */
	int64_t update_uptime; /* Uptime of last reference */
};

/**
 * @brief Structure holding gnss configuration
 *
 */
struct gnss_configuration {
	/* Single or double scan - look at lr11xx_gnss_scan_mode_t for detailed description
	 * LR11XX_GNSS_SCAN_MODE_0_SINGLE_SCAN_LEGACY - Generated NAV message format = NAV1
	 * LR11XX_GNSS_SCAN_MODE_3_SINGLE_SCAN_AND_5_FAST_SCANS = 0x03 -  Generated NAV message
	 * format = NAV2 */
	lr11xx_gnss_scan_mode_t scan_mode;
	/* Search mode for GNSS scan
	 * LR11XX_GNSS_OPTION_DEFAULT -  Search all requested satellites or fail
	 * LR11XX_GNSS_OPTION_BEST_EFFORT - Add additional search if not all satellites are found */
	lr11xx_gnss_search_mode_t effort_mode;
	/* Constellation to use
	 * LR11XX_GNSS_GPS_MASK
	 * LR11XX_GNSS_BEIDOU_MASK */
	lr11xx_gnss_constellation_mask_t constellation;
	/* Included data fields, dependent on chosen scan_mode. Refer to
	 * lr11xx_gnss_result_fields_e for more info */
	uint8_t input_parameters;
	/* Max number of satellites to detect */
	uint8_t max_sv;
	/* Assistance position */
	lr11xx_gnss_solver_assistance_position_t assistance_position;
	/* Reference GPS time */
	struct gnss_gps_ref_time ref_time;
};

static struct gnss_configuration prv_cfg = {
	.scan_mode = LR11XX_GNSS_SCAN_MODE_3_SINGLE_SCAN_AND_5_FAST_SCANS,
	.effort_mode = LR11XX_GNSS_OPTION_BEST_EFFORT,
	.input_parameters = LR11XX_GNSS_RESULTS_DOPPLER_ENABLE_MASK |
			    LR11XX_GNSS_RESULTS_DOPPLER_MASK | LR11XX_GNSS_RESULTS_BIT_CHANGE_MASK,
	.constellation = LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK,
	.max_sv = GNSS_MAX_SV,
	.assistance_position = {.latitude = 0, .longitude = 0},
	.ref_time = {.gps_time = 0, .update_uptime = 0},
};

struct gnss_data {
	/* Number of detected satellites */
	uint8_t n_sv_detected;
	/* Detected satellites  data */
	lr11xx_gnss_detected_satellite_t sv_detected[GNSS_MAX_SV];
	/* NAV payload */
	uint16_t nav_result_size;
	uint8_t nav[NAV_MAX_LENGTH];
};

static struct gnss_data prv_data;

/* External results handler */
gnss_results_handler_t prv_gnss_results_handler = NULL;

static volatile bool irq_fired = false;
static volatile bool scan_done = false;

static void prv_gnss_data_reset(void)
{
	memset(&prv_data, 0, sizeof(struct gnss_data));
}

static void prv_gnss_get_results(const void *context)
{
	/* Reset result struct */
	prv_gnss_data_reset();

	/* Get timings */
	lr11xx_gnss_timings_t gnss_timing = {0};
	int ret = lr11xx_gnss_get_timings(context, &gnss_timing);
	if (ret) {
		LOG_ERR("Failed to get timings.");
	}

	LOG_INF("Timings:");
	LOG_INF("  - radio: %u ms", gnss_timing.radio_ms);
	LOG_INF("  - computation: %u ms", gnss_timing.computation_ms);

	/* Get satellite data */
	ret = lr11xx_gnss_get_nb_detected_satellites(context, &prv_data.n_sv_detected);
	if (ret) {
		LOG_ERR("Failed to get nr. of satellites.");
		goto send;
	}

	if (prv_data.n_sv_detected > prv_cfg.max_sv) {
		LOG_WRN("More SV detected than configured (detected %u, max is %u)",
			prv_data.n_sv_detected, prv_cfg.max_sv);
		prv_data.n_sv_detected = prv_cfg.max_sv;
	}

	if (prv_data.n_sv_detected > 0) {
		ret = lr11xx_gnss_get_detected_satellites(context, prv_data.n_sv_detected,
							  prv_data.sv_detected);
		if (ret) {
			LOG_ERR("Failed to get detected satellites.");
			goto send;
		}
	}

	/* Get NAV payload */
	ret = lr11xx_gnss_get_result_size(context, &prv_data.nav_result_size);
	if (ret) {
		LOG_ERR("Failed to get result size.");
		goto send;
	}
	LOG_INF("NAV payload size: %u", prv_data.nav_result_size);

	if (prv_data.nav_result_size > NAV_MAX_LENGTH) {
		LOG_ERR("Result size too long (size %u, max is %u)", prv_data.nav_result_size,
			NAV_MAX_LENGTH);
		ret = -ERANGE;
		goto send;
	} else if (prv_data.nav_result_size > 0) {
		ret = lr11xx_gnss_read_results(context, prv_data.nav, prv_data.nav_result_size);
		if (ret) {
			LOG_ERR("Failed to read results.");
			goto send;
		}
	}

	LOG_INF("Detected %u SV(s):", prv_data.n_sv_detected);
	for (uint8_t index_sv = 0; index_sv < prv_data.n_sv_detected; index_sv++) {
		const lr11xx_gnss_detected_satellite_t *local_sv = &prv_data.sv_detected[index_sv];
		LOG_DBG("  - SV %u: CNR: %i, doppler: %i", local_sv->satellite_id, local_sv->cnr,
			local_sv->doppler);
	}

	LOG_INF("DBG message:");
#ifdef CONFIG_DEBUG_MODE
	for (uint8_t i = 0; i < prv_data.nav_result_size; i++) {
		printk("%02X ", prv_data.nav[i]);
	}
	printk("\n");
#endif /* CONFIG_DEBUG_MODE */

send:
	/* If defined call handler */
	if (prv_gnss_results_handler) {
		prv_gnss_results_handler(ret);
	}
}

/* IRQ callbacks */

/**
 * @brief IRQ callback for radio dio irq
 *
 * @param[in] dev
 */
static void prv_radio_on_dio_irq(const struct device *dev)
{
	irq_fired = true;
	LOG_DBG("Irq fired");
}

static void prv_on_gnss_scan_done(void)
{
	LOG_INF("GNSS scan done");
	scan_done = true;
}

/**
 * @brief Attach and enable interrupt for gnss scan done event
 *
 * @param[in] context
 */
static void prv_gnss_enable_irq(const void *context)
{
	lr11xx_board_attach_interrupt(context, prv_radio_on_dio_irq);
	lr11xx_board_enable_interrupt(context);
}

static void prv_gnss_irq_process(const void *context, lr11xx_system_irq_mask_t irq_filter_mask)
{
	if (irq_fired) {
		irq_fired = false;

		lr11xx_system_irq_mask_t irq_regs;
		lr11xx_system_get_and_clear_irq_status(context, &irq_regs);

		LOG_DBG("Interrupt flags = 0x%08X", irq_regs);

		irq_regs &= irq_filter_mask;

		LOG_DBG("Interrupt flags (after filtering) = 0x%08X", irq_regs);

		if ((irq_regs & LR11XX_SYSTEM_IRQ_GNSS_SCAN_DONE) ==
		    LR11XX_SYSTEM_IRQ_GNSS_SCAN_DONE) {
			LOG_INF("GNSS scan done");
			prv_on_gnss_scan_done();
		}
	}
}

static int prv_gnss_setup(const void *context)
{
	LOG_INF("Set dio irq mask");
	int ret = lr11xx_system_set_dio_irq_params(context, IRQ_MASK, 0);
	if (ret) {
		LOG_ERR("Failed to set dio irq params.");
		return ret;
	}

	LOG_INF("Clear irq status");
	ret = lr11xx_system_clear_irq_status(context, LR11XX_SYSTEM_IRQ_ALL_MASK);
	if (ret) {
		LOG_ERR("Failed to set dio irq params.");
		return ret;
	}

	irq_fired = false;
	scan_done = false;

	/* Attach new callback for gnss */
	prv_gnss_enable_irq(context);

	return 0;
}

static uint32_t prv_get_gps_time(void)
{
	return prv_cfg.ref_time.gps_time +
	       (uint32_t)(k_uptime_get() - prv_cfg.ref_time.update_uptime) / 1000;
}

void gnss_scan_autonomous(const void *context, uint8_t max_sv,
			  lr11xx_gnss_constellation_mask_t constellation)
{
	/* GNSS IRQ setup */
	LOG_INF("Setup gnss");
	int ret = prv_gnss_setup(context);
	if (ret) {
		LOG_ERR("lr11xx setup for gnss failed");
		goto err;
	}

	/* Check max number of satellites */
	if (max_sv > GNSS_MAX_SV) {
		max_sv = GNSS_MAX_SV;
	}

	prv_cfg.max_sv = max_sv;

	/* Check desired constellation */
	if (constellation > (LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK) ||
	    constellation == 0) {
		constellation = LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK;
	}

	prv_cfg.constellation = constellation;

	/* Set constellation to use */
	ret = lr11xx_gnss_set_constellations_to_use(context, prv_cfg.constellation);

	/* Init scan mode */
	ret = lr11xx_gnss_set_scan_mode(context, prv_cfg.scan_mode);
	if (ret) {
		LOG_ERR("Failed to init autonomous gnss scan.");
		goto err;
	}

	/* Call scan */
	ret = lr11xx_gnss_scan_autonomous(context, 0, prv_cfg.effort_mode, prv_cfg.input_parameters,
					  prv_cfg.max_sv);
	if (ret) {
		LOG_ERR("Failed to call autonomous gnss scan.");
		goto err;
	}

	int64_t start = k_uptime_get();

	k_sleep(K_SECONDS(2));

	while (!scan_done && k_uptime_get() - start < GNSS_TIMEOUT_S * MSEC_PER_SEC) {
		prv_gnss_irq_process(context, IRQ_MASK);
		k_sleep(K_MSEC(10));
	}

	if (!scan_done) {
		ret = -ETIMEDOUT;
		goto err;
	}

	prv_gnss_get_results(context);
err:

	if (ret) {
		LOG_INF("We failed to perform GNSS. Notify.");
		if (prv_gnss_results_handler) {
			prv_gnss_results_handler(ret);
		}
	}
}

void gnss_scan_assisted(const void *context, uint8_t max_sv,
			lr11xx_gnss_constellation_mask_t constellation)
{
	/* GNSS setup */
	LOG_INF("Setup gnss");
	int ret = prv_gnss_setup(context);
	if (ret) {
		LOG_ERR("lr11xx setup for gnss failed");
		goto err;
	}

	/* Check max number of satellites */
	if (max_sv > GNSS_MAX_SV) {
		max_sv = GNSS_MAX_SV;
	}

	prv_cfg.max_sv = max_sv;

	/* Check desired constellation */
	if (constellation > (LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK) ||
	    constellation == 0) {
		constellation = LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK;
	}

	prv_cfg.constellation = constellation;

	/* Set constellation to use */
	ret = lr11xx_gnss_set_constellations_to_use(context, prv_cfg.constellation);

	/* Init scan mode */
	ret = lr11xx_gnss_set_scan_mode(context, prv_cfg.scan_mode);
	if (ret) {
		LOG_ERR("Failed to init autonomous gnss scan.");
		goto err;
	}

	/* Set assistance position */
	ret = lr11xx_gnss_set_assistance_position(context, &prv_cfg.assistance_position);
	if (ret) {
		LOG_ERR("Failed to init assistance position.");
		goto err;
	}

	/* Call scan  - time EvaTODO */
	ret = lr11xx_gnss_scan_assisted(context, prv_get_gps_time(), prv_cfg.effort_mode,
					prv_cfg.input_parameters, prv_cfg.max_sv);
	if (ret) {
		LOG_ERR("Failed to call assisted gnss scan.");
		goto err;
	}

	int64_t start = k_uptime_get();

	k_sleep(K_SECONDS(2));

	while (!scan_done && k_uptime_get() - start < GNSS_TIMEOUT_S * MSEC_PER_SEC) {
		prv_gnss_irq_process(context, IRQ_MASK);
		k_sleep(K_MSEC(10));
	}

	if (!scan_done) {
		ret = -ETIMEDOUT;
		goto err;
	}

	prv_gnss_get_results(context);
err:

	if (ret) {
		LOG_INF("We failed to perform GNSS. Notify.");
		if (prv_gnss_results_handler) {
			prv_gnss_results_handler(ret);
		}
	}
}

void gnss_results_handler_register(gnss_results_handler_t handler)
{
	prv_gnss_results_handler = handler;
}

void gnss_scan_set_ref_position(lr11xx_gnss_solver_assistance_position_t assistance_position)
{
	prv_cfg.assistance_position = assistance_position;
}

void gnss_scan_set_ref_gps_time(uint32_t gps_time)
{
	prv_cfg.ref_time.gps_time = gps_time;
	prv_cfg.ref_time.update_uptime = k_uptime_get();
}

uint8_t gnss_get_last_nav_data(uint8_t *nav)
{
	memcpy(nav, prv_data.nav, prv_data.nav_result_size);

	return prv_data.nav_result_size;
}

uint8_t gnss_get_last_sat_data(uint8_t *sat, uint8_t *n_sat)
{
	*n_sat = prv_data.n_sv_detected;

	/* Store satellite data in byte array message */
	for (uint8_t i = 0; i < prv_data.n_sv_detected; i++) {
		sat[2 * i] = prv_data.sv_detected[i].satellite_id;
		sat[2 * i + 1] = prv_data.sv_detected[i].cnr;
	}

	return prv_data.n_sv_detected * DATA_PER_SAT;
}
