#include <charging_voltage_adjustment.h>

#include <zephyr/ztest.h>

ZTEST_SUITE(charging_voltage_adjustment_test_suite, NULL, NULL, NULL, NULL, NULL);

/**
 * @brief Test charging voltage adjustment with a full array of measurements.
 *
 *	This test takes the measured voltages, adjusts them and compares them to the expected
 * values. The maximum allowed difference between the expected and measured value is +/- 300 mV.
 *
 */
ZTEST(charging_voltage_adjustment_test_suite, test_charging_voltage_adjust_array)
{
	int voltage_expected[29] = {4000,  5000,  6000,  7000,  8000,  9000,  10000, 11000,
				    12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000,
				    20000, 21000, 22000, 23000, 24000, 25000, 26000, 27000,
				    28000, 29000, 30000, 31000, 32000};

	int voltage_measured[28] = {3212,  4023,  4734,  5545,  6556,  7567,  9078,
				    10089, 11100, 12111, 13122, 14133, 15144, 16255,
				    17286, 18377, 19588, 20799, 21610, 22821, 23732,
				    24793, 26254, 27265, 28011, 29120, 30111, 31309};

	int err = 0;
	for (int i = 0; i < 28; i++) {
		err = charging_voltage_adjust(&voltage_measured[i]);

		zassert_equal(err, 0, "Result should be 0, but is %d", err);

		zassert_between_inclusive(voltage_measured[i], voltage_expected[i] - 300,
					  voltage_expected[i] + 300,
					  "Result should be between %d and %d, but is %d. (i=%d)",
					  voltage_expected[i] - 300, voltage_expected[i] + 300,
					  voltage_measured[i], i);
	}
}

ZTEST(charging_voltage_adjustment_test_suite, test_charging_voltage_adjustment_single)
{
	int voltage_adjusted = 5345;
	int err = charging_voltage_adjust(&voltage_adjusted);

	zassert_equal(err, 0, "Result should be 0, but is %d", err);

	zassert_between_inclusive(voltage_adjusted, 6000, 7000,
				  "Result should be between 6000 and 7000, but is %d",
				  voltage_adjusted);
}

ZTEST(charging_voltage_adjustment_test_suite, test_charging_voltage_allowed_underflow)
{
	int voltage_adjusted = 2800;
	int err = charging_voltage_adjust(&voltage_adjusted);

	zassert_equal(err, 0, "Result should be 0, but is %d", err);

	zassert_between_inclusive(voltage_adjusted, 3500, 4500,
				  "Result should be between 3500 and 4500, but is %d",
				  voltage_adjusted);
}

ZTEST(charging_voltage_adjustment_test_suite, test_charging_voltage_allowed_overflow)
{
	int voltage_adjusted = 31400;
	int err = charging_voltage_adjust(&voltage_adjusted);

	zassert_equal(err, 0, "Result should be 0, but is %d", err);

	zassert_between_inclusive(voltage_adjusted, 30000, 31000,
				  "Result should be between 30000 and 31000, but is %d",
				  voltage_adjusted);
}

ZTEST(charging_voltage_adjustment_test_suite, test_charging_voltage_too_low)
{
	int voltage_adjusted = 2161;
	int err = charging_voltage_adjust(&voltage_adjusted);

	zassert_equal(err, -1, "Result should be -1, but is %d", err);
}

ZTEST(charging_voltage_adjustment_test_suite, test_charging_voltage_too_high)
{
	int voltage_adjusted = 32000;
	int err = charging_voltage_adjust(&voltage_adjusted);

	zassert_equal(err, -2, "Result should be -2, but is %d", err);
}
