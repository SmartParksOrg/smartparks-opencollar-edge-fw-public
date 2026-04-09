#include "type_conversion.h"

// Helper functions
uint32_t bytes_to_uint32_t(uint8_t *bytes)
{
	uint32_t data = bytes[3] << 24 | bytes[2] << 16 | bytes[1] << 8 | bytes[0];
	return data;
}

int32_t bytes_to_int32_t(uint8_t *bytes)
{
	int32_t data = bytes[3] << 24 | bytes[2] << 16 | bytes[1] << 8 | bytes[0];
	return data;
}

uint16_t bytes_to_uint16_t(uint8_t *bytes)
{
	uint16_t data = bytes[1] << 8 | bytes[0];
	return data;
}

int16_t bytes_to_int16_t(uint8_t *bytes)
{
	int16_t data = bytes[1] << 8 | bytes[0];
	return data;
}

uint8_t bytes_to_uint8_t(uint8_t *bytes)
{
	uint8_t data = bytes[0];

	return data;
}

int8_t bytes_to_int8_t(uint8_t *bytes)
{
	int8_t data = bytes[0];

	return data;
}

void bytes_to_float(uint8_t *bytes, int16_t data[])
{
	data[0] = bytes[1] << 8 | bytes[0];
	data[1] = bytes[3] << 8 | bytes[2];
}

void uint32_t_to_bytes(uint8_t *bytes, uint32_t data)
{
	bytes[3] = data >> 24;
	bytes[2] = data >> 16;
	bytes[1] = data >> 8;
	bytes[0] = data;
}

void uint16_t_to_bytes(uint8_t *bytes, uint16_t data)
{
	bytes[1] = data >> 8;
	bytes[0] = data;
}

void uint8_t_to_bytes(uint8_t *bytes, uint8_t data)
{
	bytes[0] = data;
}

void int8_t_to_bytes(uint8_t *bytes, int8_t data)
{
	bytes[0] = data;
}

void int16_t_to_bytes(uint8_t *bytes, int16_t data)
{
	bytes[1] = data >> 8;
	bytes[0] = data;
}

void int32_t_to_bytes(uint8_t *bytes, int32_t data)
{
	bytes[3] = data >> 24;
	bytes[2] = data >> 16;
	bytes[1] = data >> 8;
	bytes[0] = data;
}

void float_to_bytes(uint8_t *bytes, int16_t data[2])
{
	bytes[3] = data[1] >> 8;
	bytes[2] = data[1] & 0xff;
	bytes[1] = data[0] >> 8;
	bytes[0] = data[0] & 0xff;
}

void bool_to_bytes(uint8_t *bytes, bool data)
{
	bytes[0] = data;
}

void byte_array_to_bytes(uint8_t *bytes, uint8_t *data, uint8_t length)
{
	memcpy(bytes, data, length);
}
