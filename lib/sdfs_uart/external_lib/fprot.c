/***************************************************************************
 * fprot.c
 * Copyright (C) 2012 Artekit Italy
 * http://www.artekit.eu

#	Anyone is free to copy, modify, publish, use, compile, sell, or
#	distribute this software, either in source code form or as a compiled
#	binary, for any purpose, commercial or non-commercial, and by any
#	means.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
#	IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
#	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
#	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
#	OTHER DEALINGS IN THE SOFTWARE.

 *
 *	This file is part of the Artekit FPROT library for the AK-SDFS-UART device
 *
***************************************************************************/

#include "fprot.h"

#define fprot_reload_timeout() (fprot_timeout_counter = 0)
#define fprot_on_timeout()     ((fprot_timeout_counter >= FPROT_TIMEOUT_TIME) ? 1 : 0)

#ifndef NULL
#define NULL ((void *)0)
#endif /* NULL */

static txdatafunc *fprot_tx;                /*!< @brief TX function */
static rxdatafunc *fprot_rx;                /*!< @brief RX function */
static delayfunc *fprot_delay;              /*!< @brief Delay function */
static unsigned long fprot_timeout_counter; /*!< @brief Timeout accumulator */
static FPROT_HEADER fprot_header;           /*!< @brief Shared header */
static unsigned char fprot_last_error = 0;  /*!< @brief Last error received */

#define byte_swap2(val) (((val & 0xff) << 8) | ((val & 0xff00) >> 8))
#define byte_swap4(val)                                                                            \
	(((val & 0xff) << 24) | ((val & 0xff00) << 8) | ((val & 0xff0000) >> 8) |                  \
	 ((val & 0xff000000) >> 24))

/*!
 * @brief Delay between missing character reception.
 *
 * Used while waiting for packets (response, ACK, etc.).
 *
 * It will also increment fprot_timeout_counter for each delay,
 * so when fprot_timeout_counter reaches FPROT_TIMEOUT_TIME, fprot_get_packet() will
 * declare timeout and cancel the reception.
 *
 * @param milliseconds Milliseconds
 *
 * @return Nothing.
 *
 */
static void fprot_do_delay(unsigned long milliseconds)
{
	(fprot_delay)(milliseconds);
	fprot_timeout_counter++;
}

/*!
 * @brief Retrieves the length in bytes of a string
 *
 * @param str		Null-terminated string.
 *
 * @return The length of the string.
 *
 */
static unsigned short fprot_strlen(char *str)
{
	unsigned short count = 0;
	char *ptr = str;
	while (*ptr++ != 0) {
		count++;
	}
	return count;
}

/*!
 * Calculates the CRC 16 of the data parameter.
 *
 * @param data 		Pointer to a buffer.
 * @param len		Length of the buffer.
 * @param partial	Partial CRC16.
 *
 * @return The calculated CRC16.
 *
 */
static unsigned short fprot_crc16(void *data, unsigned long len, unsigned short partial)
{
	unsigned short crc = partial;
	unsigned long i;
	unsigned char j, c;
	unsigned char *ptr = (unsigned char *)data;

	for (i = 0; i < len; i++) {
		c = ((crc >> 8) & 0x00FF);
		c ^= *ptr++;
		crc = ((unsigned short)c << 8) | (crc & 0x00FF);
		for (j = 0; j < 8; j++) {
			if (crc & 0x8000) {
				crc <<= 1;
				crc ^= 0x1021;
			} else {
				crc <<= 1;
			}
		}
	}

	return crc;
}

/*!
 * @brief Assembles and transmits a packet.
 *
 * @param cmd		Command code to be sent.
 * @param file		File handle the command makes reference.
 * @param data		Pointer to the data to be sent (optional).
 * @param data_len	Length in bytes of the data parameter contents. Used only if the data
 * parameter is not null.
 *
 * @return 1 on success, 0 otherwise.
 *
 */
static void fprot_send(unsigned char cmd, FPROT_FILE file, void *data, unsigned short data_len)
{
	/* Assemble header */
	FPROT_HEADER header;

	header.preamble[0] = 'A';
	header.preamble[1] = 'K';
	header.cmd = cmd;
	header.opt = file;
	header.data_len = data_len & 0xFF;
	header.data_len |= data_len & 0xFF00;

#ifdef FPROT_BIG_ENDIAN
	header.data_len = byte_swap2(header.data_len);
#endif

	/* CRC16 for header */
	header.packet_crc16 = fprot_crc16(&header, sizeof(FPROT_HEADER) - 2, 0);

	/* If we have data */
	if (data && data_len) {
		/* Get CRC16 for the data */
		header.packet_crc16 = fprot_crc16(data, data_len, header.packet_crc16);
	}

#ifdef FPROT_BIG_ENDIAN
	header.packet_crc16 = byte_swap2(header.packet_crc16);
#endif /* FPROT_BIG_ENDIAN */

	/* Transmit header */
	(fprot_tx)((unsigned char *)&header, sizeof(FPROT_HEADER) - 2);

	/* If there is data, transmit it */
	if (data && data_len) {
		(fprot_tx)((unsigned char *)data, data_len);
	}

	/* Transmit CRC16 */
	(fprot_tx)((unsigned char *)&header.packet_crc16, 2);
}

/*!
 * @brief Reads a packet from the serial port.
 *
 * @param header	Pointer to a __FPROT_HEADER structure, used to save the incoming packet
 *header.
 * @param data		Pointer to a space of memory, used to save the incoming packet data.
 * @param datasize	Length in bytes of the space in memory pointed by buffer.
 *
 * @return 	FPROT_NO_ERROR on success, FPROT_PACKET_ERROR on packet error or FPROT_RX_TIMEOUT
 *			on timeout.
 *
 */
static unsigned char fprot_get_packet(FPROT_HEADER *header, void *data, unsigned short datasize)
{
	unsigned short offset = 0; /* Offset for packet data reception */
	unsigned short crc;        /* Calculated CRC16 */
	unsigned char b = 0;       /* Byte read */
	unsigned char step = 0;    /* Reception step */
	unsigned char *ptr = (unsigned char *)data;

	for (b = 0; b < sizeof(FPROT_HEADER); b++) {
		*((unsigned char *)header) = 0;
	}

	fprot_reload_timeout();

	/* While not going into timeout */
	while (!fprot_on_timeout()) {
		/* Read a byte */
		if ((fprot_rx)(&b, 1) == 1) {
			/* Byte received, reload timeout counter */
			fprot_reload_timeout();

			/* Switch the reception step */
			switch (step) {
			/* Preamble, must be "AK" */
			case 0:
				if (b == 'A') {
					header->preamble[0] = b;
					step++;
				}
				break;

			case 1:
				if (b == 'K') {
					header->preamble[1] = b;
					step++;
				}
				break;

			/* Command */
			case 2:
				header->cmd = b;
				step++;
				break;

			/* File handle/options */
			case 3:
				header->opt = b;
				step++;
				break;

			/* Data length Low */
			case 4:
				header->data_len = b;
				step++;
				break;

			/* Data length High */
			case 5:
				header->data_len |= b << 8;

				/* If the packet has no data jump to
				 * CRC 16 reception.
				 */
				if (!header->data_len) {
					step = 7;
					break;
				}

				/* There is data on the packet. Check if the
				 * provided buffer is not null. Otherwise return.
				 */
				if (!ptr || !datasize) {
					step = 0;
					break;
				}

				/* Check if the buffer can contain the total
				 * packet data.
				 */
				if (header->data_len > datasize) {
					/* Buffer too small */
					step = 0;
					break;
				} else {
					/* Receive data into the provided buffer,
					 * starting on the next byte reception.
					 */
					offset = 0;
					step++;
				}
				break;

			case 6:
				/* Incoming packet data body */
				ptr[offset++] = b;

				/* Check for end of packet data */
				if (offset == header->data_len) {
					/* End of data, go to CRC16 */
					step++;
				}
				break;

			case 7: /* CRC16 low */
				header->packet_crc16 = b;
				step++;
				break;

			case 8: /* CRC16 high */
				header->packet_crc16 |= b << 8;

#ifdef FPROT_BIG_ENDIAN
				header->data_len = byte_swap2(header->data_len);
#endif

				/* CRC for header */
				crc = fprot_crc16(header, sizeof(FPROT_HEADER) - 2, 0);

#ifdef FPROT_BIG_ENDIAN
				header->data_len = byte_swap2(header->data_len);
#endif

				/* If data, calculate the CRC16 for it */
				if (header->data_len) {
					crc = fprot_crc16(ptr, header->data_len, crc);
				}

				/* Check if CRC16 matches */
				if (crc == header->packet_crc16) {
					/* Packet OK */
					return FPROT_NO_ERROR;
				} else {
					/* Packet error */
					return FPROT_PACKET_ERROR;
				}
				break;
			}
		} else {
			/* No character received, delay 1 millisecond */
			fprot_do_delay(1);
		}
	}

	return FPROT_RX_TIMEOUT;
}

/*!
 * @brief Init function.
 *
 * Call this function before any other fprot_xxx function.
 *
 * @param tx		Pointer to a txdatafunc type function. The referenced function
 * 					must actually transmit data over the serial port. Called
 *every time an ACK or command must be sent.
 *
 * @param rx		Pointer to a rxdatafunc type function. The referenced function
 * 					must actually receive data from the serial port. Called
 *every time an answer is expected after sending a command.
 *
 * @param delay		Pointer to a delayfunc type function. Used to produce a delay when
 *					there are not characters to read on the serial port.
 *
 * @retval FPROT_NO_ERROR on success.
 * @retval FPROT_INVALID_PARAMS if one or more input parameters are NULL.
 */
unsigned char fprot_init(txdatafunc *tx, rxdatafunc *rx, delayfunc *delay)
{
	if (tx == NULL || rx == NULL || delay == NULL) {
		return FPROT_INVALID_PARAMS;
	}

	fprot_tx = tx;
	fprot_rx = rx;
	fprot_delay = delay;
	return FPROT_NO_ERROR;
}

/*!
 * @brief Sends a commands and waits for the answer.
 *
 * @param cmd		Command code.
 * @param file		File handle.
 * @param data		Pointer to the data to be sent (optional).
 * @param data_len	Length in bytes of the data parameter contents. Used only if the data
 *parameter is not null.
 *
 * @return 	FPROT_NO_ERROR on success, FPROT_PACKET_ERROR on packet error or FPROT_RX_TIMEOUT
 *			on timeout.
 *
 */
static unsigned char fprot_send_cmd(unsigned char cmd, FPROT_FILE file, void *data_out,
				    unsigned short data_out_len, void *data_in,
				    unsigned short data_in_len)
{
	unsigned char result = 0;

	fprot_send(cmd, file, data_out, data_out_len);

	/* wait for response */
	result = fprot_get_packet(&fprot_header, data_in, data_in_len);

	/* check result */
	if (result == FPROT_NO_ERROR) {
		if (fprot_header.cmd & FPROT_ACK_FLAG) {
			/* Remove ACK flag */
			fprot_header.cmd &= ~(FPROT_ACK_FLAG);
			return FPROT_NO_ERROR;

		} else if (fprot_header.cmd == FPROT_ERROR) {
			fprot_last_error = fprot_header.opt;
			return fprot_last_error;
		}

		return FPROT_PACKET_ERROR;
	}

	return result;
}

/*!
 * @brief Opens a file.
 *
 * @param file		Null-terminated string containing the FULL path of the file to open.
 * 					Example of a full path string: "\directory\file.txt".
 * 					The path must always begin with a '\' character.
 *
 * @param options	Opening options. Can be a combination of the following values:
 *
 * @param handle	Pointer to an 8 bit variable used to store the opened file handle.
 *					This will have a value between 1 and 4, or the
 *FPROT_INVALID_FILE value on error.
 *
 * @return FPROT_NO_ERROR on success, otherwise an error code.
 *
 */
unsigned char fprot_open(char *file, unsigned char options, FPROT_FILE *handle)
{
	unsigned char result;
	*handle = FPROT_INVALID_FILE;

	result = fprot_send_cmd(FPROT_OPEN, options, file, fprot_strlen(file), NULL, 0);
	if (result != FPROT_NO_ERROR) {
		return result;
	}

	*handle = fprot_header.opt;
	return FPROT_NO_ERROR;
}

/*!
 * @brief Closes a file.
 *
 * Closes a file opened with the fprot_open() function. If there is any pending data
 * to be saved on the file, it will be automatically flushed with this command.
 *
 * @param file		A file handle returned with the fprot_open() function.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code.
 *
 */
unsigned char fprot_close(FPROT_FILE file)
{
	unsigned char result;

	result = fprot_send_cmd(FPROT_CLOSE, file, NULL, 0, NULL, 0);
	if (result != FPROT_NO_ERROR) {
		return result;
	}

	if (fprot_header.opt == file) {
		return FPROT_NO_ERROR;
	}

	return FPROT_PACKET_ERROR;
}

/*!
 * @brief Reads data from a file.
 *
 * Call this function to read data from a file. If the data to be read is more than 512 bytes,
 * successive read commands will be sent until the requested quantity of bytes are read.
 *
 * @param file		A file handle returned with the fprot_open() function.
 * @param buf		Pointer to a buffer to receive the data.
 * @param qty		Quantity of bytes to read.
 * @param read		Pointer to a 32-bit value to receive the quantity of bytes read.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code. FPROT_EOF may be returned
 *			if the End Of File was reached.
 *
 */
unsigned char fprot_read(FPROT_FILE file, void *buf, unsigned long qty, unsigned long *read)
{
	unsigned short to_read = 0;
	unsigned long size = qty;
	unsigned long offset = 0;
	unsigned char result;
	unsigned char *ptr = (unsigned char *)buf;

	while (qty && offset < size) {
		if (qty > 512) {
			to_read = 512;
		} else {
			to_read = (unsigned short)qty;
		}

		result = fprot_send_cmd(FPROT_READ, file, &to_read, 2, ptr + offset, to_read);
		if (result != FPROT_NO_ERROR) {
			return result;
		}

		offset += fprot_header.data_len;
		qty -= fprot_header.data_len;

		/* Check if the data read is less than requested.
		 * If so, we may be over EOF.
		 */
		if (fprot_header.data_len < to_read) {
			if (read) {
				*read = offset;
			}
			return FPROT_EOF;
		}
	}

	if (read) {
		*read = offset;
	}
	return FPROT_NO_ERROR;
}

/*!
 * @brief Reads a line of text from a file.
 *
 * The AK-SDFS-UART module will start reading from the file until CRLF is found.
 *
 * This function reads a line of text from a file. If the quantity of bytes
 * to read is less than the current line of text in the file, the function will
 * return FPROT_NO_ERROR and \c buf will point to the read data (even if does not contains
 * a line of text).
 *
 * When the End Of Line is reached, the function will return FPROT_EOL and \c buf will point
 * to the read line of text.
 *
 * @param file		A file handle returned with the fprot_open() function.
 * @param buf		Pointer to a buffer to receive the data.
 * @param qty		Quantity of bytes to read (max. 512)
 * @param read		Pointer to a 16-bit value to receive the quantity of bytes read.
 *
 * @return 	FPROT_EOL if the End Of Line was reached. FPROT_EOF if the End Of File was reached.
			FPROT_NO_ERROR if the 512 bytes limit has been reached.
 *
 */
unsigned char fprot_read_line(FPROT_FILE file, void *buf, unsigned short qty, unsigned short *read)
{
	unsigned char result;
	unsigned char *ptr = (unsigned char *)buf;

	if (qty > 512) {
		return FPROT_INVALID_LENGTH;
	}

	result = fprot_send_cmd(FPROT_READ_LINE, file, &qty, 2, ptr, qty);
	if (result != FPROT_NO_ERROR) {
		return result;
	}

	if (read) {
		*read = fprot_header.data_len;
	}

	if (fprot_header.data_len == 0) {
		return FPROT_EOF;
	}
	if (fprot_header.data_len < qty) {
		return FPROT_EOL;
	}

	return FPROT_NO_ERROR;
}

/*!
 * @brief Writes data to a file.
 *
 * Call this function to write data to a file. If the data to be written is more than 512 bytes,
 * successive write commands will be sent until the requested quantity of bytes are written.
 * The data may not be written immediately. To avoid data loss it is convenient to call
 * the fprot_flush() function after writing or periodically. A fprot_close() call also
 * ensures no data loss.
 *
 * @param file		A file handle returned with the fprot_open() function.
 * @param buf		Pointer to a buffer containing the data to be written.
 * @param qty		Quantity of bytes to write.
 * @param written	Pointer to a 32-bit value to receive the quantity of actually written bytes.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code.
 *
 */
unsigned char fprot_write(FPROT_FILE file, void *buf, unsigned long qty, unsigned long *written)
{
	unsigned short write = 0;
	unsigned short writt = 0;
	unsigned char result;

	if (!written || !buf) {
		return FPROT_INVALID_PARAMS;
	}
	*written = 0;

	while (qty) {
		if (qty > 512) {
			write = 512;
		} else {
			write = (unsigned short)qty;
		}

		result = fprot_send_cmd(FPROT_WRITE, file, (unsigned char *)buf + *written, write,
					&writt, 2);
		if (result != FPROT_NO_ERROR) {
			return result;
		}

#ifdef FPROT_BIG_ENDIAN
		writt = byte_swap2(writt);
#endif /* FPROT_BIG_ENDIAN */

		*written += writt;
		if (writt != write) {
			break;
		}
		qty -= write;
	}

	return FPROT_NO_ERROR;
}

/*!
 * @brief Flushes pending data to be written on the file.
 *
 * Call this function periodically or after writing to a file to ensure
 * the data is written on the file; because the AK-SDFS-UART device may temporally store the
 * data to be written in RAM.
 *
 * @param file		A file handle returned with the fprot_open() function.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code.
 *
 */
unsigned char fprot_flush(FPROT_FILE file)
{
	return fprot_send_cmd(FPROT_FLUSH, file, NULL, 0, NULL, 0);
}

/*!
 * @brief Deletes a file.
 *
 * Deletes a file from the file system. The file should not be opened at the
 * time of calling this function.
 *
 * @param file		Null-terminated string containing the full path of the file
 *					to delete.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code.
 *
 */
unsigned char fprot_delete(char *file)
{
	return fprot_send_cmd(FPROT_DELETE, 0, file, fprot_strlen(file), NULL, 0);
}

/*!
 * @brief Checks whether is there a SD in the slot or not.
 *
 * @return 	FPROT_NO_ERROR on success, FPROT_NO_CARD if there is no SD in the
 * SD slot, or an error code on failure.
 *
 */
unsigned char fprot_check(void)
{
	unsigned char check;
	unsigned char result;

	result = fprot_send_cmd(FPROT_CHECK, 0, NULL, 0, &check, 1);
	if (result != FPROT_NO_ERROR) {
		return result;
	}

	if (check == 0) {
		return FPROT_NO_CARD;
	}

	return FPROT_NO_ERROR;
}

/*!
 * @brief Closes all opened files
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code on failure.
 *
 */
unsigned char fprot_close_all(void)
{
	return fprot_send_cmd(FPROT_CLOSE_ALL, 0, NULL, 0, NULL, 0);
}

/*!
 * @brief Gets the capacity and the current free space for the SD.
 *
 * @param capacity	A pointer to a 32-bit variable to receive the total capacity of the SD card.
 * @param free		A pointer to a 32-bit variable to receive the current free space of the SD
 * card.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code on failure.
 *
 */
unsigned char fprot_fat_info(unsigned long *capacity, unsigned long *free)
{
	unsigned long info[2];
	unsigned char result;

	result = fprot_send_cmd(FPROT_FAT_INFO, 0, NULL, 0, &info, sizeof(info));
	if (result != FPROT_NO_ERROR) {
		return result;
	}

	if (capacity) {
#ifdef FPROT_BIG_ENDIAN
		*capacity = byte_swap4(info[0]);
#else
		*capacity = info[0];
#endif /* FPROT_BIG_ENDIAN */
	}

	if (free) {
#ifdef FPROT_BIG_ENDIAN
		*free = byte_swap4(info[1]);
#else
		*free = info[1];
#endif /* FPROT_BIG_ENDIAN */
	}

	return FPROT_NO_ERROR;
}

/*!
 * @brief Asks for the quantity of currently opened files.
 *
 * @param max	A pointer to a 8-bit variable to receive the maximum quantity of concurrent
 * 				opened files.
 * @param used	A pointer to a 8-bit variable to receive the current quantity of opened files.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code on failure.
 *
 */
unsigned char fprot_status(unsigned char *max, unsigned char *used)
{
	unsigned char files[2];
	unsigned char result;

	result = fprot_send_cmd(FPROT_STATUS, 0, NULL, 0, files, 2);
	if (result != FPROT_NO_ERROR) {
		return result;
	}

	if (max) {
		*max = files[0];
	}
	if (used) {
		*used = files[1];
	}

	return FPROT_NO_ERROR;
}

/*!
 * @brief Asks for file information.
 *
 * @param file	A file handle returned with the fprot_open() function.
 * @param pos	A pointer to a 32-bit variable to receive the current position of the file pointer.
 * @param used	A pointer to a 32-bit variable to receive the total file size.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code on failure.
 *
 */
unsigned char fprot_file_info(FPROT_FILE file, unsigned long *pos, unsigned long *size)
{
	unsigned char result;
	unsigned long res[2];

	result = fprot_send_cmd(FPROT_FILE_INFO, file, NULL, 0, res, sizeof(res));
	if (result != FPROT_NO_ERROR) {
		return result;
	}

	if (pos) {
#ifdef FPROT_BIG_ENDIAN
		*pos = byte_swap4(res[0]);
#else
		*pos = res[0];
#endif /* FPROT_BIG_ENDIAN */
	}

	if (size) {
#ifdef FPROT_BIG_ENDIAN
		*size = byte_swap4(res[1]);
#else
		*size = res[1];
#endif /* FPROT_BIG_ENDIAN */
	}

	return FPROT_NO_ERROR;
}

/*!
 * @brief Creates a directory.
 *
 * The indicated directory must be provided with the full path. Example: to create the "dir1"
 * directory in the "\example\path\" directory, call this function and pass the string
 * "\example\path\dir1".
 *
 * @param path	A pointer to a null-terminated string containing the full path of the directory
 * 				to be created.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code on failure.
 *
 */
unsigned char fprot_mkdir(char *path)
{
	return fprot_send_cmd(FPROT_MKDIR, 0, path, fprot_strlen(path), NULL, 0);
}

/*!
 * @brief Moves the file pointer to an absolute position.
 *
 * If the file was opened in "write" mode and the provided position is beyond
 * the current file size, the file will be grown to fit the new position. Beware
 * that the contents of the new created size is unknown. This can be used to quickly
 * create large files.
 *
 * The new position must be checked after calling this function to ensure the
 * requested position has been reached.
 *
 * @param file		A file handle returned with the fprot_open() function.
 * @param pos		Absolute position, in bytes.
 * @param new_pos	A pointer to a 32-bit variable to receive the resulting pointer
 * 					position.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code on failure.
 *
 */
unsigned char fprot_seek(FPROT_FILE file, unsigned long pos, unsigned long *new_pos)
{
#ifdef FPROT_BIG_ENDIAN
	unsigned char ret;
	pos = byte_swap4(pos);

	ret = fprot_send_cmd(FPROT_SEEK, file, &pos, 4, new_pos, sizeof(unsigned long));

	if (ret == FPROT_NO_ERROR && new_pos) {
		*new_pos = byte_swap4(*new_pos);
	}

	return ret;

#else

	return fprot_send_cmd(FPROT_SEEK, file, &pos, 4, new_pos, sizeof(unsigned long));

#endif /* FPROT_BIG_ENDIAN */
}

/*!
 * @brief List the files and directories of a given path.
 *
 * Call this function to retrieve the list of files and directories from a path
 * of the SD card.
 *
 * This function will return the name of a file or a directory each time it is called.
 * This means, that this function must be called until the provided \c dir variable
 * is empty.
 *
 * To reset the directory scanning and to start again the listing, call this
 * function with the \c path and \c dir parameters set to NULL. After this, call
 * this function again with the desired path.
 *
 * Example:
 * @code
 *	char dir[512];
 *  unsigned char res;
 *
 * 	res = fprot_dir(NULL, NULL, 0);
 *	if (res != FPROT_NO_ERROR)
 *	{
 *		memset(dir, 0, 512);
 *		res = fprot_dir("\\example\\path", dir, 512);
 *
 *		if (res != FPROT_NO_ERROR)
 *		{
 *			if (strlen(dir) == 0)
 *			{
 *				// End of listing.
 *			} else {
 *				// Process returned "dir" variable.
 *			}
 *		}
 *	}
 * @code
 *
 * @param path		Pointer to a null-terminated string containing the path to retrieve the
 *listing. Can be NULL, and using this value will reset the current listing.
 * @param dir		Pointer to a null-terminated string to receive the name of a file or
 *directory.
 * @param dir_len	The size in bytes of the provided \c dir parameter.
 *
 * @return 	FPROT_NO_ERROR on success, otherwise an error code on failure.
 *
 */
unsigned char fprot_dir(char *path, char *dir, unsigned short dir_len)
{
	unsigned char result;

	if (path) {
		result = fprot_send_cmd(FPROT_DIR, 0, path, fprot_strlen(path), dir, dir_len);
	} else {
		result = fprot_send_cmd(FPROT_DIR, 0, NULL, 0, dir, dir_len);
	}

	if (result != FPROT_NO_ERROR) {
		return result;
	}

	return FPROT_NO_ERROR;
}

/*!
 * @brief Sets the baud rate
 *
 * Call this function to set the communications baud rate.
 * The AK-SDFS-UART device will start using the new baudrate after ACKing the command.
 *
 * @param baudrate	New baudrate, one of the following values:
 *	@arg 1200
 *	@arg 2400
 *	@arg 4800
 *	@arg 9600
 *	@arg 19200
 *	@arg 38400
 *	@arg 57600
 *	@arg 115200
 *
 * @return FPROT_NO_ERROR on success, otherwise an error code.
 *
 */
unsigned char fprot_set_baudrate(unsigned long baudrate)
{

#ifdef FPROT_BIG_ENDIAN
	baudrate = byte_swap4(baudrate);
#endif

	return fprot_send_cmd(FPROT_BAUDRATE, 0, &baudrate, 4, NULL, 0);
}

/*!
 * @brief Saves permanently parameters on the internal flash.
 *
 * Call this function to permanently save parameters (like baud rate).
 *
 * @return FPROT_NO_ERROR on success, otherwise an error code.
 *
 */
unsigned char fprot_save_parameters(void)
{
	return fprot_send_cmd(FPROT_SAVE_PARAMS, 0, NULL, 0, NULL, 0);
}

/*!
 * @brief Sets the internal AK-SDFS-UART device date and time.
 *
 * Call this function to configure the date and time to be used with the file system (when creating
 * or modifying files).
 *
 * Note that the AK-SDFS-UART board will lose the date and time after power down or reset.
 *
 * @param day		Day.
 * @param month		Month.
 * @param year		Year (offset from 2000).
 * @param hour		Hour.
 * @param min		Minutes.
 * @param seconds	Seconds.
 *
 * @return FPROT_NO_ERROR on success, otherwise an error code.
 *
 */
unsigned char fprot_set_time(unsigned char day, unsigned char month, unsigned char year,
			     unsigned char hour, unsigned char min, unsigned char seconds)
{
	unsigned char data[6];

	data[0] = year;
	data[1] = month;
	data[2] = day;
	data[3] = hour;
	data[4] = min;
	data[5] = seconds;

	return fprot_send_cmd(FPROT_SET_TIME, 0, data, 6, NULL, 0);
}

/*!
 * @brief Returns the last detected error.
 *
 * @return The code of the last detected error.
 */
unsigned char fprot_get_last_error(void)
{
	return fprot_last_error;
}
