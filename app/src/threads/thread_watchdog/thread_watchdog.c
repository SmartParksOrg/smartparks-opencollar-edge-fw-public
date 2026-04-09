#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "definitions.h"

#include "thread_watchdog.h"

#define WDT_MIN_WINDOW 0
#define WDT_MAX_WINDOW 45 * MSEC_PER_SEC

#define WDT_FEED_INTERVAL K_MSEC(WDT_MAX_WINDOW / 2 - 1000)

/* WDT device */
// Temp
#ifdef CONFIG_WATCHDOG
static const struct device *wdt = DEVICE_DT_GET_ANY(nordic_nrf_wdt);
#else
static const struct device *wdt;
#endif // CONFIG_WATCHDOG

// Variables
int wdt_channel_id;

int64_t main_thread_report_time = 0;
int64_t sensor_thread_report_time = 0;
int64_t lr_gps_thread_report_time = 0;
int64_t flash_thread_report_time = 0;

LOG_MODULE_REGISTER(thread_watchdog, 4);

/*!
 * @brief Watchdog callback. Called when watchdog expires.
 *
 * @param[in] wdt_dev         Watchdog device
 * @param[in] channel_id      Watchdog channel
 *
 * @retval  void.
 */
static void wdt_callback(const struct device *wdt_dev, int channel_id)
{
	static bool handled_event;

	if (handled_event) {
		return;
	}

	wdt_feed(wdt_dev, channel_id);

	LOG_ERR("Handled things..ready to reset\n");
	LOG_INF("WTD reboot!\n");
	handled_event = true;
}

/*!
 * @brief Watchdog timer handler. Called periodically by wtd timer.
 * It will check if all threads reported back and if yes feed watchdog.
 *
 * @retval  void.
 */
static void wdt_logic_handler(struct k_timer *dummy)
{
	// Check on last reported times for threads
	// MAIN
	if (k_uptime_get() - main_thread_report_time > THREAD_MAIN_MAX_RESPONSE * MSEC_PER_SEC) {
		// Trigger watchdog
		return;
	}

	// SENSORS
	if (k_uptime_get() - sensor_thread_report_time >
	    THREAD_SENSOR_MAX_RESPONSE * MSEC_PER_SEC) {
		// Trigger watchdog
		return;
	}

	// LR & GPS
	if (k_uptime_get() - lr_gps_thread_report_time >
	    THREAD_LR_GPS_MAX_RESPONSE * MSEC_PER_SEC) {
		// Trigger watchdog
		return;
	}

	// Flash
	if (k_uptime_get() - flash_thread_report_time > THREAD_FLASH_MAX_RESPONSE * MSEC_PER_SEC) {
		// Trigger watchdog
		return;
	}

	// If everything is ok, feed watchdog
	wdt_feed(wdt, wdt_channel_id);
}

// Define timer to feed watchdog
K_TIMER_DEFINE(wdt_feed_logic_timer, wdt_logic_handler, NULL);

int init_watchdog(void)
{
	// Check device
	if (!wdt) {
		LOG_ERR("Cannot get WDT device");
		return -ENODEV;
	}
	// Check if ready
	if (!device_is_ready(wdt)) {
		LOG_ERR("%s: device not ready.", wdt->name);
		return -ENODEV;
	}

	// WDT settings
	struct wdt_timeout_cfg wdt_config = {
		/* Reset SoC when watchdog timer expires. */
		.flags = WDT_FLAG_RESET_SOC,

		/* Expire watchdog after max window */
		.window.min = WDT_MIN_WINDOW,
		.window.max = WDT_MAX_WINDOW,
	};

	// Set up watchdog callback. Jump into it when watchdog expired.
	wdt_config.callback = wdt_callback;

	// Setup WDT
	int wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id < 0) {
		LOG_ERR("Watchdog install error");
		return wdt_channel_id;
	}

	int err = wdt_setup(wdt, 0);
	if (err < 0) {
		LOG_ERR("Watchdog setup error");
		return err;
	}

	LOG_INF("Watchdog initialized!");

	main_thread_report_time = k_uptime_get();
	sensor_thread_report_time = k_uptime_get();
	lr_gps_thread_report_time = k_uptime_get();
	flash_thread_report_time = k_uptime_get();

	/* Start feed timer */
	k_timer_start(&wdt_feed_logic_timer, K_SECONDS(0), WDT_FEED_INTERVAL);
	LOG_INF("Start Watchdog feeding timer.");
	return 0;
}

void main_thread_report(void)
{
	main_thread_report_time = k_uptime_get();
}

void sensor_thread_report(void)
{
	sensor_thread_report_time = k_uptime_get();
}

void lr_gps_thread_report(void)
{
	lr_gps_thread_report_time = k_uptime_get();
}

void flash_thread_report(void)
{
	flash_thread_report_time = k_uptime_get();
}
