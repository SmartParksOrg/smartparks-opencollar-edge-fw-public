#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/types.h>

/* Change to different peripheral if needed */
#define I2C_NODE DT_NODELABEL(i2c0)

static void i2c_scan(const struct device *i2c_dev)
{
	uint8_t error = 0u;
	printk("Starting i2c scanner...\n");
	/*start and configure i2c*/
	// printk("Value of NRF_TWIM3_NS->PSEL.SCL: %ld \n", NRF_TWIM2_NS->PSEL.SCL);
	// printk("Value of NRF_TWIM3_NS->PSEL.SDA: %ld \n", NRF_TWIM2_NS->PSEL.SDA);
	// printk("Value of NRF_TWIM3_NS->FREQUENCY: %ld \n", NRF_TWIM2_NS->FREQUENCY);
	// printk("26738688 -> 100k\n");
	/*search for i2c devices*/
	for (uint8_t i = 1; i <= 255; i++) {
		struct i2c_msg msgs[1];
		uint8_t dst = 1;
		/* Send the address to read from */
		msgs[0].buf = &dst;
		msgs[0].len = 1U;
		msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;
		error = i2c_transfer(i2c_dev, &msgs[0], 1, i);
		if (error == 0) {
			printk("0x%2x FOUND\n", i);
		} else {
			// printk("0x%2x error %d \n", i, error);
		}
	}
}

void main(void)
{
#if DT_NODE_HAS_STATUS(I2C_NODE, okay)
	// Init device
	const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
	if (i2c_dev) {
		count0 = i2c_scan(i2c_dev, found0);
	}
#else
	printk("Selected ")
#endif

	if (err) {
		return;
	}

	while (1) {
		i2c_scan();
		k_msleep(1000); // Don't pound too hard on the I2C bus
	}
}
