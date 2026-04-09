/** @file t5838.c
 *
 * @brief Microphone t5838 interface
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas. All rights reserved.
 */

#include <zephyr/audio/dmic.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "t5838_interface.h"

LOG_MODULE_REGISTER(t5838_interface);

#define BYTES_PER_SAMPLE sizeof(int16_t)
/* Size of a block for 100 ms of audio data. */
#define BLOCK_SIZE(_sample_rate, _number_of_channels)                                              \
	(BYTES_PER_SAMPLE * (_sample_rate / 10) * _number_of_channels)

/* Driver will allocate blocks from this slab to receive audio data into them.
 * Application, after getting a given block from the driver and processing its
 * data, needs to free that block.
 */
#define MAX_BLOCK_SIZE   BLOCK_SIZE(CONFIG_T5838_SAMPLE_RATE, 2)
#define SAMPLE_BIT_WIDTH 16
K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, CONFIG_T5838_BLOCK_COUNT, 4);

int t5838_init(void)
{
	const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(pdm0));
	int ret;

	if (!device_is_ready(dmic_dev)) {
		LOG_ERR("%s is not ready", dmic_dev->name);
		return 0;
	}

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
				.min_pdm_clk_freq = 1000000,
				.max_pdm_clk_freq = 3500000,
				.min_pdm_clk_dc = 40,
				.max_pdm_clk_dc = 60,
			},
		.streams = &stream,
		.channel =
			{
				.req_num_streams = 1,
			},
	};

	cfg.channel.req_num_chan = CONFIG_T5838_CHANNELS;
	/* Mono - Stereo */
	if (CONFIG_T5838_CHANNELS == 1) {
		cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
	} else {
		cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT) |
					      dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
	}
	cfg.streams[0].pcm_rate = CONFIG_T5838_SAMPLE_RATE;
	cfg.streams[0].block_size = BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan);

	/* Configure driver */
	ret = dmic_configure(dmic_dev, &cfg);
	if (ret < 0) {
		LOG_ERR("Failed to configure the driver: %d", ret);
		return ret;
	}
	LOG_INF("Microphone successfully initialized");

	return ret;
}
