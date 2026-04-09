/***************************************************************************
 * fprot.h
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

#ifndef FPROT_H_
#define FPROT_H_

#include <time.h>

/* ---->
 * Edit here to set the timeout time.
 <---- */
#define FPROT_TIMEOUT_TIME 1000

/* ---->
 * Uncomment the following line for big-endian
 <---- */
// #define FPROT_BIG_ENDIAN

typedef struct fprogHeaderTag {

	unsigned char preamble[2];
	unsigned char cmd;
	unsigned char opt;
	unsigned short data_len;
	unsigned short packet_crc16;

} FPROT_HEADER;

/*
 * Remote commands
 */
#define FPROT_OPEN        1    /* Opens a file */
#define FPROT_CLOSE       2    /* Closes a file */
#define FPROT_READ        3    /* Reads a file */
#define FPROT_READ_LINE   4    /* Reads a line */
#define FPROT_WRITE       5    /* Write a line */
#define FPROT_FLUSH       6    /* Flushes a file buffers */
#define FPROT_FILE_INFO   7    /* File info (pointer/size) */
#define FPROT_SEEK        8    /* Seek */
#define FPROT_DELETE      9    /* Deletes a file or directory */
#define FPROT_MKDIR       10   /* Creates a directory */
#define FPROT_DIR         11   /* Directory listing */
#define FPROT_CHECK       12   /* Card presence/absence */
#define FPROT_FAT_INFO    13   /* Gets FAT info (free/total space) */
#define FPROT_STATUS      14   /* Quantity of opened files */
#define FPROT_BAUDRATE    15   /* Set/Get baudrate */
#define FPROT_CLOSE_ALL   16   /* Closes all opened files */
#define FPROT_SAVE_PARAMS 17   /* Saves parameters permanently */
#define FPROT_SET_TIME    18   /* Sets the internal RTC */
#define FPROT_ACK_FLAG    0x80 /* ACK flag */
#define FPROT_ERROR                                                                                \
	0x7F /* General error, the opt/file parameter contains a one of the                        \
	      * values defined here below */

/*
 * Error codes
 */
#define FPROT_NO_ERROR        0  /* No error */
#define FPROT_NO_MORE_FILES   1  /* Max. quantity of opened files reached */
#define FPROT_FILE_NOT_FOUND  2  /* File not found */
#define FPROT_INVALID_HANDLE  3  /* The provided handle is invalid */
#define FPROT_NO_FILESYSTEM   4  /* FAT file system cannot be found */
#define FPROT_INTERNAL        5  /* Internal error */
#define FPROT_DISK_ERROR      6  /* Low level IO error */
#define FPROT_DISK_NOT_READY  7  /* Physical driver not ready */
#define FPROT_TIMEOUT         8  /* Current operation resulted in timeout */
#define FPROT_INVALID_NAME    9  /* File/directory invalid name */
#define FPROT_PATH_NOT_FOUND  10 /* Path not found */
#define FPROT_WRITE_PROTECTED 11 /* Physical drive is write protected */
#define FPROT_ALREADY_EXISTS  12 /* File/directory already exists */
#define FPROT_LOCKED          13 /* The file/directory is locked */
#define FPROT_DENIED          14 /* Access denied */
#define FPROT_INVALID_LENGTH  15 /* Read/Write length is invalid (0 or greater than 512) */
#define FPROT_PACKET_ERROR    16 /* Packet error (unexpected, malformed, invalid ACK, etc.) */
#define FPROT_NO_CARD         17 /* There is no card in slot */
#define FPROT_INVALID_PARAMS  18 /* Invalid parameters */
#define FPROT_UNKNOWN_COMMAND 19 /* Unknown command */

#define FPROT_RX_TIMEOUT 30 /* RX timeout */
#define FPROT_EOF        31 /* End Of File detected */
#define FPROT_EOL        32 /* End Of Line detected */

/*
 * Open modes
 */

#define FPROT_MODE_R 0x01 /* Read mode */
#define FPROT_MODE_W 0x02 /* Write mode */
#define FPROT_MODE_CREATE_NEW                                                                      \
	0x04                          /* 	If the file does not exist, it will be created.          \
										      If it exits, returns error. */
#define FPROT_MODE_CREATE_ALWAYS 0x08 /* Create always whenever the file exists or not */
#define FPROT_MODE_OPEN_ALWAYS   0x10 /* If the file does not exist, it will be created */
#define FPROT_MODE_RW            (FPROT_MODE_R | FPROT_MODE_W)

/*
 *
 */
#define FPROT_INVALID_FILE 0xFF

typedef unsigned char FPROT_FILE;

typedef void(txdatafunc)(unsigned char *, unsigned long);
typedef unsigned long(rxdatafunc)(unsigned char *, unsigned long);
typedef void(delayfunc)(unsigned long);

unsigned char fprot_init(txdatafunc *tx, rxdatafunc *rx, delayfunc *delay);
unsigned char fprot_open(char *file, unsigned char options, FPROT_FILE *handle);
unsigned char fprot_close(FPROT_FILE file);
unsigned char fprot_read(FPROT_FILE file, void *buf, unsigned long qty, unsigned long *read);
unsigned char fprot_read_line(FPROT_FILE file, void *buf, unsigned short qty, unsigned short *read);
unsigned char fprot_write(FPROT_FILE file, void *buf, unsigned long qty, unsigned long *written);
unsigned char fprot_flush(FPROT_FILE file);
unsigned char fprot_delete(char *file);
unsigned char fprot_check(void);
unsigned char fprot_file_info(FPROT_FILE file, unsigned long *pos, unsigned long *size);
unsigned char fprot_seek(FPROT_FILE file, unsigned long pos, unsigned long *new_pos);
unsigned char fprot_fat_info(unsigned long *capacity, unsigned long *free);
unsigned char fprot_status(unsigned char *max, unsigned char *used);
unsigned char fprot_mkdir(char *path);
unsigned char fprot_dir(char *path, char *entry, unsigned short entrylen);
unsigned char fprot_close_all(void);
unsigned char fprot_set_baudrate(unsigned long baudrate);
unsigned char fprot_save_parameters(void);
unsigned char fprot_get_last_error(void);
unsigned char fprot_set_time(unsigned char day, unsigned char month, unsigned char year,
			     unsigned char hour, unsigned char min, unsigned char seconds);

#endif /* FPROT_H_ */
