#ifndef ALMANAC_H
#define ALMANAC_H

#include <zephyr/kernel.h>

/**
 * @brief Type for function that will handle almanac update done event
 * The parameters are:
 * int err - update error code
 * uint16_t age - current age, if cannot be obtained negative error code
 */
typedef void (*almanac_update_handler_t)(int err, uint16_t age);

/**
 * @brief Register external callback for almanac update done event
 *
 */
void almanac_update_handler_register(almanac_update_handler_t);

/**
 * @brief Update buffer with almanac data for single satellite.
 *
 * @param[in] data satellite data buffer
 * @param[in] sat_idx satellite index
 * @return int number of bytes updated or negative error code
 */
int almanac_replace_single_satellite_data(uint8_t *data, uint8_t sat_idx);

/**
 * @brief Update almanac.
 *
 * @param[in] context
 * @retval 0 - success
 * @retval negative error code - failure
 */
int almanac_update(const void *context);

#endif /* ALMANAC_H */
