/**
 * @file lr_satellite.h
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#ifndef TRANSMIT_RULE_H
#define TRANSMIT_RULE_H

#include "country_code.h"
#include <zephyr/kernel.h>

enum transmit_rule {

	RxOnly, // RX only
	Test,   // limited by phy in all bands (for test house)

	kETSI_868,       // <14dBm per plane radiated within bands with duty cycle limits
	kETSI_SBAND,     // limited by phy within bands
	kFCC_915,        // limited by phy within bands
	kFCC_915_LRFHSS, // further restrict lrfhss modulation parameters

	kAUS_SBAND // Australia specific SBAND
};

enum modulation {

	Cw,
	Lora,
	Lrfhss
};

enum transmit_rule country_to_transmit_rule(enum country_code, uint32_t freq, uint32_t bw,
					    enum modulation modulation);

#endif /* TRANSMIT_RULE_H */
