

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <common_utils.h>

LOG_MODULE_REGISTER(outdoor_detection_tests);

ZTEST_SUITE(outdoor_detection_module, NULL, NULL, NULL, NULL, NULL);

ZTEST(outdoor_detection_module, test_mean_dev_z_20)
{
	/* Calculated average temperature from 20 fix cycles (Each cycle consists of 3-4 inter-fix
	 * temperature samples)
	 * Data acquired from test_data/features.csv
	 * Temperature Averages of fix cycles: [26 45] */
	float temperature_history[] = {18.6274509803922, 24.5098039215686, 25.8823529411765,
				       25.8823529411765, 25.7516339869281, 25.4901960784314,
				       26.2745098039216, 26.2745098039216, 26.6666666666667,
				       26.8627450980392, 27.0588235294118, 27.3202614379085,
				       27.843137254902,  28.0392156862745, 27.843137254902,
				       27.6470588235294, 26.4705882352941, 26.4705882352941,
				       22.3529411764706, 18.2352941176471

	};

	float temperature_inter_fix[] = {18.4313725490196, 17.6470588235294, 17.6470588235294,
					 17.6470588235294};

	/* Expected output */
	float expected_mean_dev_z_20 = -2.86944013619255;

	/* Call the function to be tested */

	float calculated_mean_dev_z_20 = 0.0f;

	int err = common_utils_mean_dev_z_20(
		temperature_history, ARRAY_SIZE(temperature_history), temperature_inter_fix,
		ARRAY_SIZE(temperature_inter_fix), &calculated_mean_dev_z_20);

	if (err) {
		LOG_ERR("Error in common_utils_mean_dev_z_20: %d", err);
	}

	zassert_within(calculated_mean_dev_z_20, expected_mean_dev_z_20, 0.005f,
		       "Calculated mean_dev_z_20 is not within the expected range");
}

ZTEST(outdoor_detection_module, test_mean_dev_z_20_1)
{
	/* Calculated average temperature from 20 fix cycles (Each cycle consists of 3-4 inter-fix
	 * temperature samples)
	 * Data acquired from test_data/features.csv
	 * Temperature Averages of fix cycles: [10 29] */
	float temperature_history[] = {19.0196078431373,
				       20,
				       26.4705882352941,
				       26.078431372549,
				       22.7450980392157,
				       22.9411764705882,
				       23.1372549019608,
				       23.1372549019608,
				       23.1372549019608,
				       22.9411764705882,
				       20.9803921568627,
				       20.1960784313725,
				       20,
				       19.2156862745098,
				       18.9542483660131,
				       18.1699346405229,
				       18.6274509803922,
				       24.5098039215686,
				       25.8823529411765,
				       25.8823529411765};

	float temperature_inter_fix[] = {26.2745098039216, 25.4901960784314, 25.4901960784314};

	/* Expected output */
	float expected_mean_dev_z_20 = 1.36520786836369;

	/* Call the function to be tested */

	float calculated_mean_dev_z_20 = 0.0f;

	int err = common_utils_mean_dev_z_20(
		temperature_history, ARRAY_SIZE(temperature_history), temperature_inter_fix,
		ARRAY_SIZE(temperature_inter_fix), &calculated_mean_dev_z_20);

	if (err) {
		LOG_ERR("Error in common_utils_mean_dev_z_20: %d", err);
	}

	zassert_within(calculated_mean_dev_z_20, expected_mean_dev_z_20, 0.005f,
		       "Calculated mean_dev_z_20 is not within the expected range");
}

ZTEST(outdoor_detection_module, test_max_z_diff)
{
	/* Data acquired from test_data/features.csv
	 * Temperature data of fix cycle 46 */
	float temperature_inter_fix[] = {18.4313725490196, 17.6470588235294, 17.6470588235294,
					 17.6470588235294};

	/* Expected output */
	float expected_max_z_diff = 1.41420973738344;

	/* Call the function to be tested */

	float calculated_max_z_diff =
		common_utils_max_z_diff(temperature_inter_fix, ARRAY_SIZE(temperature_inter_fix));

	zassert_within(calculated_max_z_diff, expected_max_z_diff, 0.00001f,
		       "Calculated max_z_diff is not within the expected range");
}

ZTEST(outdoor_detection_module, test_max_z_diff_1)
{
	/* Data acquired from test_data/features.csv
	 * Temperature data of fix cycle 24 */
	float temperature_inter_fix[] = {19.2156862745098, 19.2156862745098, 18.4313725490196};

	/* Expected output */
	float expected_max_z_diff = 0.999997450006502;

	/* Call the function to be tested */

	float calculated_max_z_diff =
		common_utils_max_z_diff(temperature_inter_fix, ARRAY_SIZE(temperature_inter_fix));

	zassert_within(calculated_max_z_diff, expected_max_z_diff, 0.00001f,
		       "Calculated max_z_diff_1 is not within the expected range");
}

ZTEST(outdoor_detection_module, test_y_z_mean)
{
	/* Data acquired from test_data/features.csv
	 * Accelerometer data of fix cycle 27 */
	struct accelerometer_data inter_fix_buf[] = {
		{1.96078431372548, -5.09803921568627, -9.01960784313725},
		{5.09803921568627, 7.45098039215687, -3.52941176470588},
		{5.09803921568627, 6.66666666666667, -3.52941176470588},
		{7.45098039215687, 5.88235294117646, -0.392156862745097}};

	size_t inter_fix_iterator = ARRAY_SIZE(inter_fix_buf);

	/* Expected output */
	float expected_y_z_mean = -0.392156862745097;

	/* Call the function to be tested */
	float calculated_y_z_mean = common_utils_y_z_mean(inter_fix_buf, inter_fix_iterator);

	zassert_within(expected_y_z_mean, calculated_y_z_mean, 0.00001f,
		       "Calculated max_z_diff_1 is not within the expected range");
}

ZTEST(outdoor_detection_module, test_x_z_max_diff)
{
	/* Data acquired from test_data/features.csv
	 * Accelerometer data of fix cycle 30 */
	struct accelerometer_data inter_fix_buf[] = {
		{3.52941176470588, 2.74509803921569, 8.23529411764706},
		{1.17647058823529, -0.392156862745097, 9.01960784313725},
		{0.392156862745097, -1.17647058823529, 9.01960784313725}};

	size_t inter_fix_iterator = ARRAY_SIZE(inter_fix_buf);

	/* Expected output */
	float expected_max_diff = -4.70588235294117;

	/* Call the function to be tested */
	float calculated_max_diff = common_utils_x_z_max_diff(inter_fix_buf, inter_fix_iterator);

	zassert_within(expected_max_diff, calculated_max_diff, 0.00001f,
		       "Calculated acc_x-z_max is not within the expected range");
}

ZTEST(outdoor_detection_module, test_x_z_max_diff_1)
{
	/* Data acquired from test_data/features.csv
	 * Accelerometer data of fix cycle 27 */
	struct accelerometer_data inter_fix_buf[] = {
		{1.96078431372548, -5.09803921568627, -9.01960784313725},
		{5.09803921568627, 7.45098039215687, -3.52941176470588},
		{5.09803921568627, 6.66666666666667, -3.52941176470588},
		{7.45098039215687, 5.88235294117646, -0.392156862745097}};

	size_t inter_fix_iterator = ARRAY_SIZE(inter_fix_buf);

	/* Expected output */
	float expected_max_diff = 10.9803921568627;

	/* Call the function to be tested */
	float calculated_max_diff = common_utils_x_z_max_diff(inter_fix_buf, inter_fix_iterator);

	zassert_within(expected_max_diff, calculated_max_diff, 0.00001f,
		       "Calculated acc_x-z_max_1 is not within the expected range");
}

ZTEST(outdoor_detection_module, test_x_z_avg_diff)
{
	/* Data acquired from test_data/features.csv
	 * Accelerometer data of fix cycle 30 */
	struct accelerometer_data inter_fix_buf[] = {
		{3.52941176470588, 2.74509803921569, 8.23529411764706},
		{1.17647058823529, -0.392156862745097, 9.01960784313725},
		{0.392156862745097, -1.17647058823529, 9.01960784313725}};

	size_t inter_fix_iterator = ARRAY_SIZE(inter_fix_buf);

	/* Expected output */
	float expected_x_z_avg_diff = -7.05882352941176;

	/* Call the function to be tested */
	float calculated_x_z_avg_diff =
		common_utils_x_z_avg_diff(inter_fix_buf, inter_fix_iterator);

	zassert_within(expected_x_z_avg_diff, calculated_x_z_avg_diff, 0.00001f,
		       "Calculated acc_x-z_mean is not within the expected range");
}

ZTEST(outdoor_detection_module, test_mag_var)
{
	/* Data acquired from test_data/features.csv
	 * Accelerometer data of fix cycle 30 */
	struct accelerometer_data inter_fix_buf[] = {
		{3.52941176470588, 2.74509803921569, 8.23529411764706},
		{1.17647058823529, -0.392156862745097, 9.01960784313725},
		{0.392156862745097, -1.17647058823529, 9.01960784313725}};

	size_t inter_fix_iterator = ARRAY_SIZE(inter_fix_buf);

	/* Expected output */
	float expected_mag_var = 0.0236502442560324;

	/* Call the function to be tested */
	float calculated_mag_value = common_utils_mag_var(inter_fix_buf, inter_fix_iterator);

	zassert_within(expected_mag_var, calculated_mag_value, 0.00001f,
		       "Calculated mag_var is not within the expected range");
}

ZTEST(outdoor_detection_module, test_mag_var_1)
{
	/* Data acquired from test_data/features.csv
	 * Accelerometer data of fix cycle 21 */
	struct accelerometer_data inter_fix_buf[] = {
		{1.96078431372548, -7.45098039215687, -9.01960784313725},
		{6.66666666666667, -5.88235294117646, -5.09803921568627},
		{1.17647058823529, -7.45098039215687, -7.45098039215687},
		{-0.392156862745097, -5.88235294117646, -6.66666666666667}};

	size_t inter_fix_iterator = ARRAY_SIZE(inter_fix_buf);

	/* Expected output */
	float expected_mag_var = 1.48467240158644;

	/* Call the function to be tested */
	float calculated_mag_value = common_utils_mag_var(inter_fix_buf, inter_fix_iterator);

	zassert_within(expected_mag_var, calculated_mag_value, 0.00001f,
		       "Calculated mag_var is not within the expected range");
}

ZTEST(outdoor_detection_module, test_x_z_sum_diff)
{
	/* Data acquired from test_data/features.csv
	 * Accelerometer data of fix cycle 30 */
	struct accelerometer_data inter_fix_buf[] = {
		{3.52941176470588, 2.74509803921569, 8.23529411764706},
		{1.17647058823529, -0.392156862745097, 9.01960784313725},
		{0.392156862745097, -1.17647058823529, 9.01960784313725}};

	size_t inter_fix_iterator = ARRAY_SIZE(inter_fix_buf);

	/* Expected output */
	float expected_x_z_sum_diff = -21.1764705882353;

	/* Call the function to be tested */
	float calculated_x_z_sum_diff =
		common_utils_x_z_sum_diff(inter_fix_buf, inter_fix_iterator);

	zassert_within(expected_x_z_sum_diff, calculated_x_z_sum_diff, 0.00001f,
		       "Calculated acc_x-z_sum is not within the expected range");
}

ZTEST(outdoor_detection_module, test_x_z_sum_diff_1)
{
	/* Data acquired from test_data/features.csv
	 * Accelerometer data of fix cycle 134 */
	struct accelerometer_data inter_fix_buf[] = {
		{5.88235294117646, -5.88235294117646, 5.09803921568627},
		{5.09803921568627, -6.66666666666667, 5.09803921568627},
		{4.31372549019608, -8.23529411764706, 4.31372549019608},
		{4.31372549019608, -7.45098039215687, 4.31372549019608}};

	size_t inter_fix_iterator = ARRAY_SIZE(inter_fix_buf);

	/* Expected output */
	float expected_x_z_sum_diff = 0.784313725490193;

	float calculated_x_z_sum_diff =
		common_utils_x_z_sum_diff(inter_fix_buf, inter_fix_iterator);

	zassert_within(expected_x_z_sum_diff, calculated_x_z_sum_diff, 0.00001f,
		       "Calculated acc_x-z_sum_1 is not within the expected range");
}
