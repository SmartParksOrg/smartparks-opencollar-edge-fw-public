/** @file vm3011_interface.c
 *
 * @brief Interface for microphone vm3011
 *
 * @par
 * COPYRIGHT NOTICE: (c) 20232 Irnas. All rights reserved.
 */

#include <stdio.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "vm3011.h"
#include "vm3011_interface.h"

#define SAMPLE_RATE      4000
#define SAMPLE_BIT_WIDTH 16
#define BYTES_PER_SAMPLE sizeof(int16_t)

/* Size of a block for _duration ms of audio data. */
#define BLOCK_SIZE(_sample_rate, _number_of_channels, _duration_ms)                                \
	(BYTES_PER_SAMPLE * _sample_rate) / (1000 / _duration_ms) * _number_of_channels

#define SLAB_BLOCK_SIZE  BLOCK_SIZE(SAMPLE_RATE, 1, 1000)
#define SLAB_BLOCK_COUNT 2
#define SLAB_ALIGNMENT   4
static K_MEM_SLAB_DEFINE(mem_slab, SLAB_BLOCK_SIZE, SLAB_BLOCK_COUNT, SLAB_ALIGNMENT);

LOG_MODULE_REGISTER(vm3011_interface);

#define MIC_VM3011_EN DT_NODELABEL(mic_en)
#if DT_NODE_EXISTS(MIC_VM3011_EN)
const struct gpio_dt_spec dmic_en_gpio_dev = GPIO_DT_SPEC_GET(MIC_VM3011_EN, gpios);
#else
const struct gpio_dt_spec dmic_en_gpio_dev;
#endif // DT_NODE_EXISTS(MIC_VM3011_EN)

const struct device *dmic_dev = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));
const struct device *vm3011_dev = DEVICE_DT_GET(DT_NODELABEL(vm3011));

void *buffers[8] = {0};

struct pcm_stream_cfg stream = {
	.pcm_width = SAMPLE_BIT_WIDTH,
	.mem_slab = &mem_slab,
};
struct dmic_cfg cfg = {
	.io =
		{
			/* These fields can be used to limit the PDM clock
			 * configurations that the driver is allowed to use
			 * to those supported by the microphone.
			 */
			.min_pdm_clk_freq = 1200000,
			.max_pdm_clk_freq = 4000000,
			.min_pdm_clk_dc = 40,
			.max_pdm_clk_dc = 60,
		},
	.streams = &stream,
	.channel =
		{
			.req_num_streams = 1,
		},
};

#ifdef CONFIG_VM3011_INT
// Work handler
static void vm3011_int_work_handler(struct k_work *work)
{
	LOG_INF("Start vm3011 worker.");
	// vm3011_start_sampling();
	vm3011_clear_dout(vm3011_dev);
	k_sleep(K_MSEC(100));
}

/* Define work handler */
K_WORK_DEFINE(vm3011_int_work, vm3011_int_work_handler);

static void vm3011_cb(const struct device *dev)
{
	LOG_INF("VM3011 PIN HIGH\n\n");
	k_work_submit(&vm3011_int_work);
}
#endif // CONFIG_VM3011_INT

int vm3011_init(void)
{
	// Init enable pin
#if DT_NODE_EXISTS(MIC_VM3011_EN)
	if (!dmic_en_gpio_dev.port) {
		LOG_ERR("Failed to initialize en pin gpio");
		return -ENOENT;
	} else {
		LOG_INF("Init mic en gpio device done.");
	}
	int err = gpio_pin_configure_dt(&dmic_en_gpio_dev, GPIO_OUTPUT_ACTIVE);

	if (err) {
		LOG_ERR("Failed to init mic en pin");
		return err;
	} else {
		LOG_INF("Init mic en pin done.");
	}

#else
	LOG_INF("Mic en pin not supported.");
#endif

	// Init I2c and pdm devices
	if (!dmic_dev) {
		LOG_ERR("DMIC dev binding failed!");
		return -ENODEV;
	}

	if (!vm3011_dev) {
		LOG_ERR("VM3011 dev binding failed!");
		return -ENODEV;
	}

	int ret;

#ifdef CONFIG_VM3011_INT
	vm3011_dout_set_handler(vm3011_dev, vm3011_cb);
#endif // CONFIG_VM3011_INT

	// Config
	cfg.channel.req_num_chan = 1;
	cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
	cfg.streams[0].pcm_rate = SAMPLE_RATE;
	cfg.streams[0].block_size =
		BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan, 1000);

	ret = dmic_configure(dmic_dev, &cfg);
	if (ret < 0) {
		LOG_ERR("Failed to configure the driver: %d", ret);
	}

	return ret;
}

int vm3011_start_sampling(void)
{
	int ret;

	// start sampling PDM
	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
	if (ret < 0) {
		LOG_ERR("START trigger failed: %d", ret);
		return ret;
	} else {
		LOG_INF("Start sampling PDM.");
	}

	// wait for a sample buffer to be available and get it
	void *buffer;
	uint32_t size;

	ret = dmic_read(dmic_dev, 0, &buffer, &size, 2000);
	if (ret < 0) {
		LOG_ERR("Read failed: %d", ret);
		return ret;
	}

	LOG_INF("Got buffer %p of %u samples (%u bytes)", buffer, size / 2, size);

	// stop sampling
	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
	if (ret < 0) {
		LOG_ERR("STOP trigger failed: %d", ret);
		return ret;
	}

	// print the buffer

	// free the buffer
	k_mem_slab_free(&mem_slab, &buffer);

	// put mic back into ZLP mode
	vm3011_clear_dout(vm3011_dev);

	return ret;
}

int vm3011_enable(bool status)
{
#if DT_NODE_EXISTS(MIC_VM3011_EN)
	if (!dmic_en_gpio_dev.port) {
		LOG_ERR("micen pin gpionot initialized!");
		return -ENOENT;
	}

	LOG_WRN("Set MIC EN pinto: %d", (int)status);
	gpio_pin_set_dt(&dmic_en_gpio_dev, (int)status);

#else
	LOG_INF("Mic en pin not supported.");
#endif
	return 0;
}
