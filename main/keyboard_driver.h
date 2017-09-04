 /* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#ifndef CHERRY8x16_H
#define CHERRY8x16_H

/*lint ++flb "Enter library region" */

#include <stdbool.h>
#include <stdint.h>

/** @file
* @brief Cherry 8x16 keyboard matrix driver
*
*
* @defgroup nrf_drivers_cherry8x16 Cherry 8x16 keyboard matrix driver
* @{
* @ingroup nrf_drivers
* @brief Cherry 8x16 keyboard matrix driver.
*/

#define CHERRY8x16_MAX_NUM_OF_PRESSED_KEYS 6 //!< Maximum number of pressed keys kept in buffers
#define CHERRY8x16_DEFAULT_KEY_LOOKUP_MATRIX (const uint8_t*)0 //!< If passed to @ref cherry8x16_init, default lookup matrix will be used

#define KEY_PACKET_MODIFIER_KEY_INDEX (0) //!< Index in the key packet where modifier keys such as ALT and Control are stored
#define KEY_PACKET_RESERVED_INDEX (1) //!< Index in the key packet where OEMs can store information
#define KEY_PACKET_KEY_INDEX (2) //!< Start index in the key packet where pressed keys are stored
#define KEY_PACKET_MAX_KEYS (6) //!< Maximum number of keys that can be stored into the key packet                                                                  
#define KEY_PACKET_SIZE (KEY_PACKET_KEY_INDEX+KEY_PACKET_MAX_KEYS) //!< Total size of the key packet in bytes
#define KEY_PACKET_NO_KEY (0) //!< Value to be stored to key index to indicate no key is pressed

bool cherry8x16_init(void);
/**
 * @brief Function for creating a new key packet if new data is available and key ghosting is not detected.
 *
 * @param p_key_packet Array that will hold the created key packet. Previously created packet will be discarded.
 * @param p_key_packet_size Key packet size in bytes.
 * @return
 * @retval true If new packet was created.
 * @retval false  If packet was not created.
 */
bool new_packet(const uint8_t ** p_key_packet, uint8_t *p_key_packet_size);
void sleep_mode_prepare(void);
/**
 *@}
 **/

/*lint --flb "Leave library region" */ 
#endif
