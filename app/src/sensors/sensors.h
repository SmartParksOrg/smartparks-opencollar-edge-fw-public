/** @file sensors.h
 *
 * @brief Interface to sensor drives, accelerometer, GPS
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <zephyr/kernel.h>

/* Init */
int sensors_init(void);

/* Disable */
void sensors_disable(void);

/* Read all sensors */
int sensors_read(void);

/* Read lis2dw */
int sensors_lis2dw12_read(void);

/* Handle all sensors */
void sensors_handle(void);

/* Check if configuration for any of the sensors was changed */
void sensors_update_settings(void);

void sensors_handle_commands(void);

#endif /* SENSORS_H */

/*** end of file ***/
