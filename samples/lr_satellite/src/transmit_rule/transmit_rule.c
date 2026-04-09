#include "transmit_rule.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#define CW_MODULATION_MASK     1
#define LORA_MODULATION_MASK   2
#define LRFHSS_MODULATION_MASK 4
#define ALL_MODULATION_MASK    7

struct band_rule {

	uint32_t min;
	uint32_t max;
	enum transmit_rule rule;
	uint32_t modulation;
};

static uint32_t prv_modulation_to_mask(enum modulation modulation)
{
	return ((uint32_t)1) << (unsigned)modulation;
}

static enum transmit_rule prv_find_rule(const struct band_rule *rules, uint8_t n_rules,
					uint32_t freq, uint32_t bw, enum modulation modulation)
{
	enum transmit_rule rule = RxOnly;

	uint32_t f_min = freq - (bw / 2);
	uint32_t f_max = freq + (bw / 2);

	uint32_t mod_mask = prv_modulation_to_mask(modulation);

	for (uint8_t i = 0; i < n_rules; i++) {

		if ((f_min >= rules[i].min) && (f_max <= rules[i].max) &&
		    ((mod_mask & rules[i].modulation) > 0)) {

			rule = rules[i].rule;
			break;
		}
	}

	return rule;
}

enum transmit_rule country_to_transmit_rule(enum country_code country, uint32_t freq, uint32_t bw,
					    enum modulation modulation)
{
	static enum transmit_rule rule = RxOnly;

	switch (country) {
	default:
		break;

	// custom code: test house conditions
	case kXTH:

		// note that this defers limits to RadioAdapter
		rule = Test;
		break;

	case kALB: // Albania
	case kDZA: // Algeria
	case kAND: // Andorra
	case kAUT: // Austria
	case kAZE: // Azerbaijan
	case kBEL: // Belgium
	case kBIH: // Bosnia
	case kBWA: // Botswana
	case kBGR: // Bulgaria
	case kHRV: // Croatia
	case kCYP: // Cyprus
	case kCZE: // Czech Republic
	case kDNK: // Denmark
	case kEST: // Estonia
	case kFIN: // Finland
	case kFRA: // France
	case kGEO: // Georgia
	case kDEU: // Germany
	case kGRC: // Greece
	case kVAT: // Vatican City
	case kHUN: // Hungary
	case kISL: // Iceland
	case kIRQ: // Iraq
	case kIRL: // Ireland
	case kITA: // Italy
	case kLVA: // Latvia
	case kLIE: // Liechtenstein
	case kLTU: // Lithuania
	case kLUX: // Luxembourg
	case kMLT: // Malta
	case kMCO: // Monaco
	case kMNE: // Montenegro
	case kNLD: // Netherlands
	case kNOR: // Norway
	case kPOL: // Poland
	case kPRT: // Portugal
	case kMDA: // Moldova
	case kROU: // Romania
	case kSMR: // San Marino
	case kSVK: // Slovakia
	case kSVN: // Slovenia
	case kZAF: // South Africa
	case kESP: // Spain
	case kSJM: // Svalbard & Jan Mayen
	case kSWE: // Sweden
	case kCHE: // Switzerland
	case kTUR: // Turkey
	case kUKR: // Ukraine
	case kGBR: // United Kingdom
	{
		static const struct band_rule table[] = {
			{862000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kARM: // Armenia
	case kBHR: // Bahrain
	case kBEN: // Benin
	case kBTN: // Bhutan
	case kCPV: // Cape Verde
	case kEGY: // Egypt
	case kFRO: // Faroe Islands
	case kFLK: // Falkland Islands
	case kGUF: // French Guiana
	case kPYF: // French Polynesia
	case kGIB: // Gibraltar
	case kLBN: // Lebanon
	case kMDG: // Madagascar
	case kMTQ: // Martinique
	case kMRT: // Mauritania
	case kMYT: // Mayotte
	case kNCL: // New Caledonia
	case kOMN: // Oman
	case kREU: // Reunion
	case kSRB: // Serbia
	case kTUN: // Tunisia
	case kWLF: // Wallis & Futuna
	case kZWE: // Zimbabwe
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kBDI:  // Burundi
	case kCIV:  // Cote d'Ivoire
	case kGNQ:  // Equatorial Guinea
	case kKEN:  // Kenya
	case kNAME: // Namibia
	case kNGA:  // Nigeria
	case kRWA:  // Rwanda
	case kWSM:  // Samoa
	case kSEN:  // Senegal
	case kZMB:  // Zambia
	{
		static const struct band_rule table[] = {
			{868000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kASM: // American Samoa
	case kBHS: // Bahamas
	case kBRB: // Barbados
	case kBLZ: // Belize
	case kBMU: // Bermuda
	case kDMA: // Dominica
	case kECU: // Ecuador
	case kGRD: // Grenada
	case kGUM: // Guam
	case kMEX: // Mexico
	case kMSR: // Montserrat
	case kMNP: // Northern Mariana Islands
	case kPAN: // Panama
	case kPRI: // Puerto Rico
	case kTO:  // Trinidad & Tobago
	{
		static const struct band_rule table[] = {
			{902000000, 928000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kAIA:  // Anguilla
	case kARG:  // Argentina
	case kBRA:  // Brazil
	case kCHL:  // Chile
	case kCOL:  // Colombia
	case kDOM:  // Dominican Republic
	case kSLV:  // El Salvador
	case kGTM:  // Guatemala
	case kHND:  // Honduras
	case kJAM:  // Jamaica
	case kNIC:  // Nicaragua
	case kNFK:  // Norfolk Island
	case kPNG:  // Papua New Guinea
	case kPRY:  // Paraguay
	case kPER:  // Peru
	case kPHL:  // Philippines
	case kSURE: // Suriname
	case kTON:  // Tonga
	case kTCA:  // Turks & Caicos Islands
	case kURY:  // Uruguay
	case kCXR:  // Christmas Island
	case kCCK:  // Cocos (Keeling) Islands
	{
		static const struct band_rule table[] = {
			{915000000, 928000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kAUS: // Australia
	{
		static const struct band_rule table[] = {
			{915000000, 928000000, kFCC_915, CW_MODULATION_MASK | LORA_MODULATION_MASK},
			{915000000, 928000000, kFCC_915_LRFHSS, LORA_MODULATION_MASK},
			{1980000000, 2010000000, kAUS_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kIND: // India
	case kNER: // Niger
	{
		static const struct band_rule table[] = {
			{865000000, 868000000, kETSI_868, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kNZL: // New Zealand
	case kNIU: // Niue
	{
		static const struct band_rule table[] = {
			{864000000, 868000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 928000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kMAC: // Macau
	case kTHA: // Thailand
	{
		static const struct band_rule table[] = {
			{920000000, 925000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kCRI: // Costa Rica
	case kJPN: // Japan
	{
		static const struct band_rule table[] = {
			{920000000, 928000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kCAN: // Canada
	case kUSA: // US
	{
		static const struct band_rule table[] = {
			{902000000, 928000000, kFCC_915, CW_MODULATION_MASK | LORA_MODULATION_MASK},
			{902000000, 928000000, kFCC_915_LRFHSS, LORA_MODULATION_MASK},
			{2000000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kBGD: // Bangladesh
	{
		static const struct band_rule table[] = {
			{866000000, 868000000, kETSI_868, ALL_MODULATION_MASK},
			{922000000, 925000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kBRN: // Brunei
	{
		static const struct band_rule table[] = {
			{866000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{920000000, 925000000, kETSI_868, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kKHM: // Cambodia
	{
		static const struct band_rule table[] = {
			{866000000, 869000000, kETSI_868, ALL_MODULATION_MASK},
			{923000000, 925000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kCOM: // Comoros
	{
		static const struct band_rule table[] = {
			{862000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 921000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kCOK: // Cook Islands
	{
		static const struct band_rule table[] = {
			{864000000, 868000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 928000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kCUB: // Cuba
	{
		static const struct band_rule table[] = {
			{915000000, 921000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kGRL: // Greenland
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 918000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kHKG: // Hong Kong
	{
		static const struct band_rule table[] = {
			{865000000, 868000000, kETSI_868, ALL_MODULATION_MASK},
			{920000000, 925000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kIDN: // Indonesia
	{
		static const struct band_rule table[] = {
			{920000000, 923000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kIRN: // Iran
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 918000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kISR: // Israel
	{
		static const struct band_rule table[] = {
			{917000000, 920000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kJOR: // Jordan
	{
		static const struct band_rule table[] = {
			{862000000, 868000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 919400000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kKAZ: // Kazakhstan
	{
		static const struct band_rule table[] = {
			{863000000, 868000000, kETSI_868, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kKWT: // Kuwait
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 921000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kLAO: // Laos
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{923000000, 925000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kMYS: // Malaysia
	{
		static const struct band_rule table[] = {
			{919000000, 924000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kMMR: // Myanmar
	{
		static const struct band_rule table[] = {
			{866000000, 869000000, kETSI_868, ALL_MODULATION_MASK},
			{919000000, 924000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kQAT: // Qatar
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 921000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kPAK: // Pakistan
	{
		static const struct band_rule table[] = {
			{865000000, 869000000, kETSI_868, ALL_MODULATION_MASK},
			{920000000, 925000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kRUS: // Russia
	{
		static const struct band_rule table[] = {
			{864000000, 869000000, kETSI_868, ALL_MODULATION_MASK},
			{916000000, 921000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kSAU: // Saudi Arabia
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 921000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kSGP: // Singapore
	{
		static const struct band_rule table[] = {
			{866000000, 869000000, kETSI_868, ALL_MODULATION_MASK},
			{920000000, 925000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kSLB: // Solomon Islands
	{
		static const struct band_rule table[] = {
			{918000000, 926000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kSOME: // Somalia
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 918000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kKOR: // South Korea
	{
		static const struct band_rule table[] = {
			{917000000, 923000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kLKA: // Sri Lanka
	{
		static const struct band_rule table[] = {
			{868000000, 869000000, kETSI_868, ALL_MODULATION_MASK},
			{920000000, 924000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kSYR: // Syria
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 921000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kTZA: // Tanzania
	{
		static const struct band_rule table[] = {
			{866000000, 869000000, kETSI_868, ALL_MODULATION_MASK},
			{920000000, 925000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kTKL: // Tokelau
	{
		static const struct band_rule table[] = {
			{864000000, 868000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 928000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kARE: // United Arab Emirates
	{
		static const struct band_rule table[] = {
			{863000000, 870000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 921000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kVUT: // Vanuatu
	{
		static const struct band_rule table[] = {
			{863000000, 869000000, kETSI_868, ALL_MODULATION_MASK},
			{915000000, 918000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 3, freq, bw, modulation);
		break;
	}

	case kVEN: // Venezuela
	{
		static const struct band_rule table[] = {
			{922000000, 928000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}

	case kVNM: // Vietnam
	{
		static const struct band_rule table[] = {
			{918000000, 923000000, kFCC_915, ALL_MODULATION_MASK},
			{1980000000, 2010000000, kETSI_SBAND, ALL_MODULATION_MASK}};
		rule = prv_find_rule(table, 2, freq, bw, modulation);
		break;
	}
	}

	return rule;
}
