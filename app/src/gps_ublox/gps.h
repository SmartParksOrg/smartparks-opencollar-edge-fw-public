/*
    gps.h - Ublox gps wrapper - this is called from main
    It inits GPIO and desired I2C.
    Then it calls gps_ublox_interface for init, get position and get time.
    Made by Vid Rajtmajer <vid@irnas.eu>, IRNAS d.o.o.
*/
#ifndef GPS_H
#define GPS_H

#include <zephyr/kernel.h>

#include "gps_ublox_interface.h"

/**
 * @brief Init GPS Ublox module.
 *
 * @return int 0 - OK, negative integer error code - FAIL
 */
int gps_init(void);

/**
 * @brief Reset GPS module
 *
 * Turn off VBCK pin. Turn on power and send GPS Ublox reset command. Turn power off and reset all
 * variables.
 *
 * @return int - 0
 */
int gps_reset(void);

/*!
 * @brief Stop GPS.
 *
 * Disable VBCK and power pin, disable device.
 *
 * EvaTODO: do we need to configure I2C pins as well?
 *
 */
void gps_stop(void);

/**
 * @brief Enable/disable communication device for Ublox and power.
 *
 * @param state
 */
void gps_power(uint8_t state);

/*!
 * @brief Return GPS Ublox module enabled flag.
 *
 * @return bool status
 */
bool gps_get_enabled(void);

/**
 * @brief Request new gps data using hot/cold fix logic.
 *
 * @return int - error code
 */
int gps_get_fix(void);

/**
 * @brief Get status of last fix.
 *
 * @return true
 * @return false
 */
bool gps_get_last_fix_success(void);

/**
 * @brief Get latest GPS fix status data.
 *
 * @param[in] fix - successful fix
 * @param[in] hot_retry - number of hot retries
 * @param[in] cold_retry - number of cold retries
 * @param[in] ttf - time to fix in seconds
 */
void gps_get_last_fix_status(bool *fix, uint8_t *hot_retry, uint8_t *cold_retry, uint16_t *ttf);

/**
 * @brief Return latest fix data structure.
 *
 * @param[in] position
 */
void gps_get_last_fix_data(struct gps_ublox_position_data *position);

/**
 * @brief Return latest fix time.
 *
 * @param[in] time
 */
void gps_get_last_fix_time(uint32_t *time);

/**
 * @brief Get active tracking flag
 *
 * @return true
 * @return false
 */
bool gps_get_active_tracking(void);

/**
 * @brief Get satellite data message.
 *
 * @param[in] payload buffer
 * @param[in] max_mayload max length
 * @return int - message length
 */
int gps_get_sat_data(uint8_t *payload, uint8_t max_mayload);

/**
 * @brief Check if it is time to perform a GPS fix
 *
 * @retval true - it is time to perform a GPS fix
 * @retval false - it is not time to perform a GPS fix
 */
bool gps_send_interval(void);

/**
 * @brief Handle GPS motion triggered events passed from lis2dw12 accelerometer sensor.
 * Increments motion detection counter and removes old timestamps.
 * Check README.md for more info.
 */
void gps_motion_triggered_event_handler(void);

/**
 * @brief Update last fix time.
 *
 * Sets the internal timestamp of the last performed GPS fix to the current time. This means that
 * the module will behave as if the last fix performed was "now". This affects the scheduling of the
 * next fix.
 */
void gps_update_last_fix_time(void);

#endif /* GPS_H */
