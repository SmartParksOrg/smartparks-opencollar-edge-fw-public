#ifndef TYPE_CONVERSION_H__
#define TYPE_CONVERSION_H__

#include <string.h>
#include <zephyr/kernel.h>

// Helper functions
uint32_t bytes_to_uint32_t(uint8_t *bytes);
int32_t bytes_to_int32_t(uint8_t *bytes);
uint16_t bytes_to_uint16_t(uint8_t *bytes);
int16_t bytes_to_int16_t(uint8_t *bytes);
int8_t bytes_to_int8_t(uint8_t *bytes);
uint8_t bytes_to_uint8_t(uint8_t *bytes);
void bytes_to_float(uint8_t *bytes, int16_t data[]);

void uint32_t_to_bytes(uint8_t *bytes, uint32_t data);
void uint16_t_to_bytes(uint8_t *bytes, uint16_t data);
void uint8_t_to_bytes(uint8_t *bytes, uint8_t data);
void int32_t_to_bytes(uint8_t *bytes, int32_t data);
void int16_t_to_bytes(uint8_t *bytes, int16_t data);
void int8_t_to_bytes(uint8_t *bytes, int8_t data);
void bool_to_bytes(uint8_t *bytes, bool data);
void float_to_bytes(uint8_t *bytes, int16_t data[2]);
void byte_array_to_bytes(uint8_t *bytes, uint8_t *data, uint8_t length);

#endif
