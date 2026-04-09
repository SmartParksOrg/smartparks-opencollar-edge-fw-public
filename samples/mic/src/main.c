/*
 * Copyright (c) 2023 Irnas d.o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/audio/dmic.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dmic_sample);

/**
 * See README.md for details on user configurable parameters, acceptable values and
 * definitions.
 */
#define SAMPLE_RATE      16000
#define SAMPLE_BIT_WIDTH 16

/* The sample will record audio of this duration in milliseconds */
#define TOTAL_RECORDING_DURATION_MS 1000

/* The audio recording will be split into blocks of this duration. Ideally,
 * TOTAL_RECORDING_DURATION_MS is a multiple of BLOCK_DURATION_MS  */
#define BLOCK_DURATION_MS 100

/* Bytes used per samples */
#define BYTES_PER_SAMPLE sizeof(int16_t)

/**
 * @brief Set the size of a block of data
 * Size of a block for 100 ms of audio data.
 *
 * NOTE: We divide the block into 10 smaller blocks to avoid buffer overflow.
 *
 * @param[in] _sample_rate Sample rate in Hz.
 * @param[in] _number_of_channels Number of channels.
 * @param[in] _block_duration_ms Duration of one block in milliseconds.
 */
#define BLOCK_SIZE(_sample_rate, _number_of_channels, _block_duration_ms)                          \
	(BYTES_PER_SAMPLE * (_sample_rate * _block_duration_ms / 1000) * _number_of_channels)

/**
 * @brief Driver will allocate blocks from this slab to receive audio data into them.
 * Application, after getting a given block from the driver and processing its
 * data, needs to free that block.
 */
#define MAX_BLOCK_SIZE BLOCK_SIZE(SAMPLE_RATE, 2, BLOCK_DURATION_MS)

/**
 * @brief Calculate the number of blocks needed for the given total recording duration.
 *
 */
#define BLOCK_COUNT TOTAL_RECORDING_DURATION_MS / BLOCK_DURATION_MS
K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 4);

/**
 * @brief do a pdm transfer sequence
 *
 * Configure the driver and start sampling
 * Samples are handled in chunks of BLOCK_DURATION - each block is freed immediately.
 * After @p block_count blocks have been read, stop sampling.
 *
 * @param[in] dmic_dev microphone device
 * @param[in] cfg configuration of the microphone
 * @param[in] block_count number of available memory blocks
 *
 * @return int 0 on success, negative error code otherwise
 */
static int do_pdm_transfer(const struct device *dmic_dev, struct dmic_cfg *cfg, size_t block_count)
{
	int ret;

	LOG_INF("PCM output rate: %u, channels: %u", cfg->streams[0].pcm_rate,
		cfg->channel.req_num_chan);

	ret = dmic_configure(dmic_dev, cfg);
	if (ret < 0) {
		LOG_ERR("Failed to configure the driver: %d", ret);
		return ret;
	}

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
	if (ret < 0) {
		LOG_ERR("START trigger failed: %d", ret);
		return ret;
	}

	for (int i = 0; i < block_count; ++i) {
		void *buffer;
		uint32_t size;
		int ret;

		ret = dmic_read(dmic_dev, 0, &buffer, &size, BLOCK_DURATION_MS * 2);
		if (ret < 0) {
			LOG_ERR("%d - read failed: %d", i, ret);
			return ret;
		}

		LOG_INF("%d - got buffer %p of %u bytes", i, buffer, size);

		/**
		 * NOTE:The purpose of this sample is to show how to use the driver, so we clear
		 * each block after processing it. If your use-case expects to process the recorded
		 * data asynchronously, you should not clear the blocks in this manner.
		 */
		k_mem_slab_free(&mem_slab, &buffer);
	}

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
	if (ret < 0) {
		LOG_ERR("STOP trigger failed: %d", ret);
		return ret;
	}

	return ret;
}

int main(void)
{
	const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(pdm0));
	int ret;

	LOG_INF("DMIC sample");

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

	/* Test the driver with different configurations. */
	/* Single channel sound capture configuration */
	cfg.channel.req_num_chan = 1;
	cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
	cfg.streams[0].pcm_rate = SAMPLE_RATE;
	cfg.streams[0].block_size =
		BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan, BLOCK_DURATION_MS);

	ret = do_pdm_transfer(dmic_dev, &cfg, BLOCK_COUNT);
	if (ret < 0) {
		return 0;
	}

	/* Dual channel sound capture configuration */
	cfg.channel.req_num_chan = 2;
	cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT) |
				      dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
	cfg.streams[0].pcm_rate = SAMPLE_RATE;
	cfg.streams[0].block_size =
		BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan, BLOCK_DURATION_MS);

	ret = do_pdm_transfer(dmic_dev, &cfg, BLOCK_COUNT);
	if (ret < 0) {
		return 0;
	}

	LOG_INF("Exiting");
	return 0;
}
