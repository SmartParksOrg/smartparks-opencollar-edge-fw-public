/******************************************************************************************
 * Copyright 2015, 2016 Ideetron B.V.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************************/
/******************************************************************************************
 *
 * File:        Encrypt_V20.cpp
 * Author:      Gerben den Hartog
 * Company:    Ideetron B.V.
 * Website:     http://www.ideetron.nl/LoRa
 * E-mail:      info@ideetron.nl
 ******************************************************************************************/
/****************************************************************************************
 *
 * Created on:           22-10-2015
 *
 * Firmware Version 1.0
 * First version
 *
 * Firmware Version 2.0
 * Works the same is 1.0 using own AES encryption
 ****************************************************************************************/

/*
*****************************************************************************************
* INCLUDE FILES
*****************************************************************************************
*/

#include "AES-128_V10.h"
#include "Encrypt_V20.h"

/*
*****************************************************************************************
* Description : Function for encrypting the payload of the package
*
* Arguments   : *Data         Data to encrypt
*               Data_Length   Length of the data
*               Frame_Counter Frame number
*               Frame_Counter_32 true if a 32-bits framecounter must be used, false if a
*                                16-bits framecounter must be used
*****************************************************************************************
*/
void Encrypt_Payload(uint8_t *Data, uint8_t Data_Length, uint32_t Frame_Counter,
		     bool Frame_Counter_32, bool UseNetworkKey, enum lora_link_direction Direction,
		     struct lorawan_auth_info *Auth_Info)
{
	uint8_t i = 0x00;
	uint8_t j;
	uint8_t Number_of_Blocks = 0x00;
	uint8_t Incomplete_Block_Size = 0x00;

	uint8_t Block_A[16];

	/* Calculate number of blocks */
	Number_of_Blocks = Data_Length / 16;
	Incomplete_Block_Size = Data_Length % 16;
	if (Incomplete_Block_Size != 0) {
		Number_of_Blocks++;
	}

	/* Preform encryption off data blocks */
	for (i = 1; i <= Number_of_Blocks; i++) {
		/* Create block A */
		Block_A[0] = 0x01;
		Block_A[1] = 0x00;
		Block_A[2] = 0x00;
		Block_A[3] = 0x00;
		Block_A[4] = 0x00;

		Block_A[5] = Direction;

		Block_A[6] = Auth_Info->DevAddr[3];
		Block_A[7] = Auth_Info->DevAddr[2];
		Block_A[8] = Auth_Info->DevAddr[1];
		Block_A[9] = Auth_Info->DevAddr[0];

		Block_A[10] = (Frame_Counter & 0x00FF);
		Block_A[11] = ((Frame_Counter >> 8) & 0x00FF);

		if (Frame_Counter_32) {
			Block_A[12] = (Frame_Counter >> 16) & 0xff; /* Frame counter upper Bytes */
			Block_A[13] = (Frame_Counter >> 24) & 0xff;
		} else {
			Block_A[12] = 0x00; /* Frame counter upper Bytes */
			Block_A[13] = 0x00;
		}

		Block_A[14] = 0x00;

		Block_A[15] = i;

		/* Calculate S */
		if (UseNetworkKey) {
			AES_Encrypt(Block_A, Auth_Info->NwkSKey);
		} else {
			AES_Encrypt(Block_A, Auth_Info->AppSKey);
		}

		/* Check for last block */
		if (i != Number_of_Blocks) {
			/* Preform encryption */
			for (j = 0; j < 16; j++) {
				*Data = *Data ^ Block_A[j];
				Data++;
			}
		} else {
			/* Check if last is full */
			if (Incomplete_Block_Size == 0) {
				Incomplete_Block_Size = 16;
			}
			/* Preform encryption */
			for (j = 0; j < Incomplete_Block_Size; j++) {
				*Data = *Data ^ Block_A[j];
				Data++;
			}
		}
	}
}

void Calculate_AES_CMAC(uint8_t *key, uint8_t *Header, uint8_t *Data, uint8_t Data_Length,
			uint8_t *Final_MIC, uint8_t NwkSkey[16])
{
	uint8_t i;
	uint8_t Key_K1[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t Key_K2[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	// uint8_t Data_Copy[16];

	uint8_t Old_Data[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t New_Data[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	uint8_t Number_of_Blocks = 0x00;
	uint8_t Incomplete_Block_Size = 0x00;
	uint8_t Block_Counter = 0x01;

	/* Calculate number of Blocks and blocksize of last block */
	Number_of_Blocks = Data_Length / 16;
	Incomplete_Block_Size = Data_Length % 16;

	/* Add one for the last incomplete block */
	if (Incomplete_Block_Size != 0) {
		Number_of_Blocks++;
	}

	/* Generate the keys used in the MIC calculation */
	Generate_Keys(Key_K1, Key_K2, NwkSkey);

	/* Preform Calculation on Block B0 */
	/* Preform AES encryption */
	AES_Encrypt(Header, key);

	/* Copy Block_B to Old_Data */
	for (i = 0; i < 16; i++) {
		Old_Data[i] = Header[i];
	}

	/* Preform full calculating until n-1 message blocks */
	while (Block_Counter < Number_of_Blocks) {
		/* Copy data into array */
		for (i = 0; i < 16; i++) {
			New_Data[i] = *Data;
			Data++;
		}

		/* Preform XOR with old data */
		XOR(New_Data, Old_Data);

		/* Preform AES encryption */
		AES_Encrypt(New_Data, key);

		/* Copy New_Data to Old_Data */
		for (i = 0; i < 16; i++) {
			Old_Data[i] = New_Data[i];
		}

		/* Raise Block counter */
		Block_Counter++;
	}

	/* Perform calculation on last block */
	/* Check if Datalength is a multiple of 16 */
	if (Incomplete_Block_Size == 0) {
		// Copy last data into array
		for (i = 0; i < 16; i++) {
			New_Data[i] = *Data;
			Data++;
		}

		/* Preform XOR with Key 1 */
		XOR(New_Data, Key_K1);

		/* Preform XOR with old data */
		XOR(New_Data, Old_Data);

		/* Preform last AES routine */
		AES_Encrypt(New_Data, key);
	} else {
		/* Copy the remaining data and fill the rest */
		for (i = 0; i < 16; i++) {
			if (i < Incomplete_Block_Size) {
				New_Data[i] = *Data;
				Data++;
			}
			if (i == Incomplete_Block_Size) {
				New_Data[i] = 0x80;
			}
			if (i > Incomplete_Block_Size) {
				New_Data[i] = 0x00;
			}
		}

		/* Preform XOR with Key 2 */
		XOR(New_Data, Key_K2);

		/* Preform XOR with Old data */
		XOR(New_Data, Old_Data);

		/* Preform last AES routine */
		AES_Encrypt(New_Data, key);
	}

	Final_MIC[0] = New_Data[0];
	Final_MIC[1] = New_Data[1];
	Final_MIC[2] = New_Data[2];
	Final_MIC[3] = New_Data[3];
}
/*
*****************************************************************************************
* Description : Function to calculate the MIC of the complete radio package
*
* Arguments   : *Data         Data of radio package
*               *Final_MIC    The array holding the MIC 4 bytes
*               Data_Length   Length of the radio package
*               Frame_Counter Number of the radio package
*               Frame_Counter_32 true if 32-bits framecounters must be used, false if 16-bits
*                                framecounters must be used.
*               Lorawan_Minor_Version LoRaWAN minor version (enum lorawan_minor_version)
*               Direction     Direction of the package (enum lora_link_direction)
*****************************************************************************************
*/
void Calculate_MIC(uint8_t *Data, uint8_t *Final_MIC, uint8_t Data_Length, uint32_t Frame_Counter,
		   bool Frame_Counter_32, enum lorawan_minor_version Lorawan_Minor_Version,
		   enum lora_link_direction Direction, struct lorawan_auth_info *Auth_Info)
{
	uint8_t Block_B[16];

	/* Create Block_B0 */
	Block_B[0] = 0x49;
	Block_B[1] = 0x00;
	Block_B[2] = 0x00;
	Block_B[3] = 0x00;
	Block_B[4] = 0x00;

	Block_B[5] = Direction;

	Block_B[6] = Auth_Info->DevAddr[3];
	Block_B[7] = Auth_Info->DevAddr[2];
	Block_B[8] = Auth_Info->DevAddr[1];
	Block_B[9] = Auth_Info->DevAddr[0];

	Block_B[10] = (Frame_Counter & 0x00FF);
	Block_B[11] = ((Frame_Counter >> 8) & 0x00FF);

	if (Frame_Counter_32) {
		Block_B[12] = (Frame_Counter >> 16) & 0xff; /* Frame counter upper bytes */
		Block_B[13] = (Frame_Counter >> 24) & 0xff;
	} else {
		Block_B[12] = 0x00; /* Frame counter upper bytes */
		Block_B[13] = 0x00;
	}

	Block_B[14] = 0x00;
	Block_B[15] = Data_Length;

	Calculate_AES_CMAC(Auth_Info->NwkSKey, Block_B, Data, Data_Length, Final_MIC,
			   Auth_Info->NwkSKey);

	if (Lorawan_Minor_Version) {
		uint8_t saved_MIC[] = {Final_MIC[0], Final_MIC[1]};
		/* Create Block_B1 */
		Block_B[0] = 0x49;

		Block_B[1] = 0; /* ConfFCnt */
		Block_B[2] = 0;

		Block_B[3] = 0; /* TxDr */
		Block_B[4] = 0; /* TxCh */

		Block_B[5] = 0; /* Dir */

		Block_B[6] = Auth_Info->DevAddr[3];
		Block_B[7] = Auth_Info->DevAddr[2];
		Block_B[8] = Auth_Info->DevAddr[1];
		Block_B[9] = Auth_Info->DevAddr[0];

		Block_B[10] = (Frame_Counter & 0x00FF);
		Block_B[11] = ((Frame_Counter >> 8) & 0x00FF);

		if (Frame_Counter_32) {
			Block_B[12] = (Frame_Counter >> 16) & 0xff; /* Frame counter upper bytes */
			Block_B[13] = (Frame_Counter >> 24) & 0xff;
		} else {
			Block_B[12] = 0x00; /* Frame counter upper bytes */
			Block_B[13] = 0x00;
		}

		Block_B[14] = 0x00;
		Block_B[15] = Data_Length;

		Calculate_AES_CMAC(Auth_Info->AppSKey, Block_B, Data, Data_Length, Final_MIC,
				   Auth_Info->NwkSKey);
		Final_MIC[2] = saved_MIC[0];
		Final_MIC[3] = saved_MIC[1];
	}
}

/*
*****************************************************************************************
* Description : Function that generates the keys used in the MIC calculation
*
* Arguments   : *K1   Key 1 16 bytes
*               *K2   Key 2 16 bytes
*****************************************************************************************
*/
void Generate_Keys(uint8_t *K1, uint8_t *K2, uint8_t NwkSKey[16])
{
	uint8_t i;
	uint8_t MSB_Key;

	/* Encrypt the zeros in K1 with the NwkSKey */
	AES_Encrypt(K1, NwkSKey);

	/* Create K1 */
	/* Check if MSB is 1 */
	if ((K1[0] & 0x80) == 0x80) {
		MSB_Key = 1;
	} else {
		MSB_Key = 0;
	}

	/* Shift K1 one bit left */
	Shift_Left(K1);

	/* if MSB was 1 */
	if (MSB_Key == 1) {
		K1[15] = K1[15] ^ 0x87;
	}

	/* Copy K1 to K2 */
	for (i = 0; i < 16; i++) {
		K2[i] = K1[i];
	}

	/* Check if MSB is 1 */
	if ((K2[0] & 0x80) == 0x80) {
		MSB_Key = 1;
	} else {
		MSB_Key = 0;
	}

	/* Shift K2 one bit left */
	Shift_Left(K2);

	/* Check if MSB was 1 */
	if (MSB_Key == 1) {
		K2[15] = K2[15] ^ 0x87;
	}
}

/*
*****************************************************************************************
* Description : Preform a left shift of 1 bit on a provided 16 byte array
*
* Arguments   : *Data   Array that will be shift
*****************************************************************************************
*/
void Shift_Left(uint8_t *Data)
{
	uint8_t i;
	uint8_t Overflow = 0;
	// uint8_t High_Byte, Low_Byte;

	for (i = 0; i < 16; i++) {
		/* Check for overflow on next byte except for the last byte */
		if (i < 15) {
			/* Check if upper bit is one */
			if ((Data[i + 1] & 0x80) == 0x80) {
				Overflow = 1;
			} else {
				Overflow = 0;
			}
		} else {
			Overflow = 0;
		}

		/* Shift one left */
		Data[i] = (Data[i] << 1) + Overflow;
	}
}

/*
*****************************************************************************************
* Description : Preform a XOR with two 16 byte arrays
*
* Arguments   : *New_Data   The new data as input in the MIC calculation
*               *Old_Data   Data from previous cycle
*****************************************************************************************
*/
void XOR(uint8_t *New_Data, uint8_t *Old_Data)
{
	uint8_t i;

	for (i = 0; i < 16; i++) {
		New_Data[i] = New_Data[i] ^ Old_Data[i];
	}
}
