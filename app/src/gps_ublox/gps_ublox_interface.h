/*
    This is an interface that connects the CPP Ublox library with the main C code.
    Added to make it possible to run Ublox lib on Zephyr (NCS)

    This port was made by Vid Rajtmajer <vid@irnas.eu>, IRNAS d.o.o.
*/

#ifndef _GPS_UBLOX_INTERFACE_H_
#define _GPS_UBLOX_INTERFACE_H_

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gps_ublox_position_data {
	int32_t latitude;
	int32_t longitude;
	int32_t altitude;
	uint8_t SIV;
	uint8_t fix_type;
	uint16_t scaled_accuracy;
	uint8_t PDOP;
	uint16_t scaled_cog;
	uint8_t scaled_sog;
};

int gps_ublox_begin_i2c(const struct device *gps_dev);
int gps_ublox_begin_serial(const struct device *gps_dev);
void gps_ublox_process_nmea_sentences(void);

int gps_ublox_get_position(struct gps_ublox_position_data *position);

int gps_ublox_get_datetime(uint32_t *time);
int gps_ublox_get_sat_data(uint8_t *payload, uint8_t max_payload);
uint8_t gps_ublox_number_of_currently_detected_satellites(void);

void gps_ublox_flush(void);
void gps_ublox_flush_data(void);
void gps_ublox_clear_buffer(void); // Clear I2C buffer - read all
void gps_ublox_reset_sat_data(void);

void gps_ublox_reset(void);

void gps_ublox_print_position_data(struct gps_ublox_position_data *position);

#ifdef __cplusplus
}
#endif

#endif // _GPS_UBLOX_INTERFACE_H_
