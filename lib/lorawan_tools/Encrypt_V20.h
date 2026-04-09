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
 * File:        Encrypt_V20.h
 * Author:      Gerben den Hartog
 * Company:    Ideetron B.V.
 * Website:     http://www.ideetron.nl/LoRa
 * E-mail:      info@ideetron.nl
 ******************************************************************************************/
/****************************************************************************************
 *
 * Created on:           22-10-2015
 *
 * Firmware Version 2.0
 * First version
 *
 * Firmware Version 2.0
 * Works the same is 1.0 using own AES encryption
 ****************************************************************************************/

#ifndef ENCRYPT_V20_H
#define ENCRYPT_V20_H

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <lorawan_tools_common.h>

/*
*****************************************************************************************
* FUNCTION PROTOTYPES
*****************************************************************************************
*/

/**
 * @brief Calculates the Message Integrity Code (MIC) for a LoRaWAN packet
 *
 * @param [in] Data Pointer to the data buffer
 * @param [out] Final_MIC Pointer to the buffer for the final MIC
 * @param [in] Data_Length Length of the data buffer
 * @param [in] Frame_Counter Frame counter value
 * @param [in] Frame_Counter_32 Whether the frame counter is 32-bit (true) or 16-bit (false)
 * @param [in] Lorawan_Minor_Version LoRaWAN minor version (enum lorawan_minor_version)
 * @param [in] Direction Direction of the packet (enum lora_link_direction)
 * @param [in] Auth_Info Pointer to the lorawan_auth_info structure containing LoRa keys and the
 * device address
 */
void Calculate_MIC(uint8_t *Data, uint8_t *Final_MIC, uint8_t Data_Length, uint32_t Frame_Counter,
		   bool Frame_Counter_32, enum lorawan_minor_version Lorawan_Minor_Version,
		   enum lora_link_direction Direction, struct lorawan_auth_info *Auth_Info);

/**
 * @brief Encrypts a LoRaWAN FRMPayload (downlink or uplink)
 *
 * @param [in out] Data The data to encrypt. The buffer is modified in-place.
 * @param [in] Data_Length data buffer length
 * @param [in] Frame_Counter 16- or 32-bit frame counter
 * @param [in] Frame_Counter_32 true if 32-bit frame counter used, else false
 * @param [in] UseNetworkKey true to use NwkSkey (FPort = 0), false to use AppSkey (FPort > 0)
 * @param [in] Direction Direction of the packet (enum lora_link_direction)
 * @param [in] Auth_Info Pointer to the lorawan_auth_info structure containing LoRa keys and the
 * device address
 */
void Encrypt_Payload(uint8_t *Data, uint8_t Data_Length, uint32_t Frame_Counter,
		     bool Frame_Counter_32, bool UseNetworkKey, enum lora_link_direction Direction,
		     struct lorawan_auth_info *Auth_Info);

/**
 * @brief Generates the keys used in MIC calculation
 *
 * @param [out] K1 Pointer to 16-byte buffer for Key 1
 * @param [out] K2 Pointer to 16-byte buffer for Key 2
 * @param [in] NwkSkey 16-byte Network Session Key
 */
void Generate_Keys(uint8_t *K1, uint8_t *K2, uint8_t NwkSkey[16]);

/**
 * @brief Shifts a 16-byte buffer left by one bit
 */
void Shift_Left(uint8_t *Data);

/**
 * @brief XORs two 16-byte buffers (New_Data ^= Old_Data)
 */
void XOR(uint8_t *New_Data, uint8_t *Old_Data);

#endif
