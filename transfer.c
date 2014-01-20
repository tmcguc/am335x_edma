/*
	An attempt to setup DMA transfer with Ping/Pong Buffering using EDMA.c
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/genalloc.h>
#include <linux/platform_data/edma.h>

#include <asm/dma.h>

#include "transfer.h"				/* Our header file */

#ifdef DEBUG
static void print_buf_info(int slot, char *name)
{
	struct edmacc_param p;
	if (slot < 0)
		return;
	edma_read_slot(slot, &p);
	printk(KERN_DEBUG "%s: 0x%x, opt=%x, src=%x, a_b_cnt=%x dst=%x\n",
			name, slot, p.opt, p.src, p.a_b_cnt, p.dst);
	printk(KERN_DEBUG "    src_dst_bidx=%x link_bcntrld=%x src_dst_cidx=%x ccnt=%x\n",
			p.src_dst_bidx, p.link_bcntrld, p.src_dst_cidx, p.ccnt);
}
#endif

struct davinci_runtime_data {
	spinlock_t lock;
	int period;				/* current DMA period */
	int asp_channel;			/* Master DMA channel */
	int asp_link[2];			/* asp parameter link channel, ping/pong */
	struct davinci_pcm_dma_params *params;	/* DMA params */
	int ram_channel;
	int ram_link;
	int ram_link2;
	struct edmacc_param asp_params;
	struct edmacc_param ram_params;
};

/*
 * Not used with ping/pong
 */
static void davinci_pcm_enqueue_dma(struct snd_pcm_substream *substream)
{
	struct davinci_runtime_data *prtd = substream->runtime->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int period_size;
	unsigned int dma_offset;
	dma_addr_t dma_pos;
	dma_addr_t src, dst;
	unsigned short src_bidx, dst_bidx;
	unsigned short src_cidx, dst_cidx;
	unsigned int data_type;
	unsigned short acnt;
	unsigned int count;
	unsigned int fifo_level;

	period_size = snd_pcm_lib_period_bytes(substream);
	dma_offset = prtd->period * period_size;
	dma_pos = runtime->dma_addr + dma_offset;
	fifo_level = prtd->params->fifo_level;

	pr_debug("davinci_pcm: audio_set_dma_params_play channel = %d "
		"dma_ptr = %x period_size=%x\n", prtd->asp_link[0], dma_pos,
		period_size);

	data_type = prtd->params->data_type;
	count = period_size / data_type;
	if (fifo_level)
		count /= fifo_level;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		src = dma_pos;
		dst = prtd->params->dma_addr;
		src_bidx = data_type;
		dst_bidx = 0;
		src_cidx = data_type * fifo_level;
		dst_cidx = 0;
	} else {
		src = prtd->params->dma_addr;
		dst = dma_pos;
		src_bidx = 0;
		dst_bidx = data_type;
		src_cidx = 0;
		dst_cidx = data_type * fifo_level;
	}

	acnt = prtd->params->acnt;
	edma_set_src(prtd->asp_link[0], src, INCR, W8BIT);
	edma_set_dest(prtd->asp_link[0], dst, INCR, W8BIT);

	edma_set_src_index(prtd->asp_link[0], src_bidx, src_cidx);
	edma_set_dest_index(prtd->asp_link[0], dst_bidx, dst_cidx);

	if (!fifo_level)
		edma_set_transfer_params(prtd->asp_link[0], acnt, count, 1, 0,
							ASYNC);
	else
		edma_set_transfer_params(prtd->asp_link[0], acnt, fifo_level,
							count, fifo_level,
							ABSYNC);
}

static void davinci_pcm_dma_irq(unsigned link, u16 ch_status, void *data)
{
	struct snd_pcm_substream *substream = data;
	struct davinci_runtime_data *prtd = substream->runtime->private_data;

	print_buf_info(prtd->ram_channel, "i ram_channel");
	pr_debug("davinci_pcm: link=%d, status=0x%x\n", link, ch_status);

	if (unlikely(ch_status != DMA_COMPLETE))
		return;

	if (snd_pcm_running(substream)) {
		spin_lock(&prtd->lock);
		if (prtd->ram_channel < 0) {
			/* No ping/pong must fix up link dma data*/
			davinci_pcm_enqueue_dma(substream);
		}
		davinci_pcm_period_elapsed(substream);
		spin_unlock(&prtd->lock);
		snd_pcm_period_elapsed(substream);
	}
}

/*
 * Only used with ping/pong.
 * This is called after runtime->dma_addr, period_bytes and data_type are valid
 */
static int ping_pong_dma_setup(struct snd_pcm_substream *substream)
{
	unsigned short ram_src_cidx, ram_dst_cidx;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct davinci_runtime_data *prtd = runtime->private_data;
	struct snd_dma_buffer *iram_dma =
		(struct snd_dma_buffer *)substream->dma_buffer.private_data;
	struct davinci_pcm_dma_params *params = prtd->params;
	unsigned int data_type = params->data_type;
	unsigned int acnt = params->acnt;
	/* divide by 2 for ping/pong */
	unsigned int ping_size = snd_pcm_lib_period_bytes(substream) >> 1;
	unsigned int fifo_level = prtd->params->fifo_level;
	unsigned int count;
	if ((data_type == 0) || (data_type > 4)) {
		printk(KERN_ERR "%s: data_type=%i\n", __func__, data_type);
		return -EINVAL;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_addr_t asp_src_pong = iram_dma->addr + ping_size;
		ram_src_cidx = ping_size;
		ram_dst_cidx = -ping_size;
		edma_set_src(prtd->asp_link[1], asp_src_pong, INCR, W8BIT);

		edma_set_src_index(prtd->asp_link[0], data_type,
				data_type * fifo_level);
		edma_set_src_index(prtd->asp_link[1], data_type,
				data_type * fifo_level);

		edma_set_src(prtd->ram_link, runtime->dma_addr, INCR, W32BIT);
	} else {
		dma_addr_t asp_dst_pong = iram_dma->addr + ping_size;
		ram_src_cidx = -ping_size;
		ram_dst_cidx = ping_size;
		edma_set_dest(prtd->asp_link[1], asp_dst_pong, INCR, W8BIT);

		edma_set_dest_index(prtd->asp_link[0], data_type,
				data_type * fifo_level);
		edma_set_dest_index(prtd->asp_link[1], data_type,
				data_type * fifo_level);

		edma_set_dest(prtd->ram_link, runtime->dma_addr, INCR, W32BIT);
	}

	if (!fifo_level) {
		count = ping_size / data_type;
		edma_set_transfer_params(prtd->asp_link[0], acnt, count,
				1, 0, ASYNC);
		edma_set_transfer_params(prtd->asp_link[1], acnt, count,
				1, 0, ASYNC);
	} else {
		count = ping_size / (data_type * fifo_level);
		edma_set_transfer_params(prtd->asp_link[0], acnt, fifo_level,
				count, fifo_level, ABSYNC);
		edma_set_transfer_params(prtd->asp_link[1], acnt, fifo_level,
				count, fifo_level, ABSYNC);
	}

	edma_set_src_index(prtd->ram_link, ping_size, ram_src_cidx);
	edma_set_dest_index(prtd->ram_link, ping_size, ram_dst_cidx);
	edma_set_transfer_params(prtd->ram_link, ping_size, 2,
			runtime->periods, 2, ASYNC);

	/* init master params */
	edma_read_slot(prtd->asp_link[0], &prtd->asp_params);
	edma_read_slot(prtd->ram_link, &prtd->ram_params);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct edmacc_param p_ram;
		/* Copy entire iram buffer before playback started */
		prtd->ram_params.a_b_cnt = (1 << 16) | (ping_size << 1);
		/* 0 dst_bidx */
		prtd->ram_params.src_dst_bidx = (ping_size << 1);
		/* 0 dst_cidx */
		prtd->ram_params.src_dst_cidx = (ping_size << 1);
		prtd->ram_params.ccnt = 1;

		/* Skip 1st period */
		edma_read_slot(prtd->ram_link, &p_ram);
		p_ram.src += (ping_size << 1);
		p_ram.ccnt -= 1;
		edma_write_slot(prtd->ram_link2, &p_ram);
		/*
		 * When 1st started, ram -> iram dma channel will fill the
		 * entire iram.  Then, whenever a ping/pong asp buffer finishes,
		 * 1/2 iram will be filled.
		 */
		prtd->ram_params.link_bcntrld =
			EDMA_CHAN_SLOT(prtd->ram_link2) << 5;
	}
	return 0;
}

/* 1 asp tx or rx channel using 2 parameter channels
 * 1 ram to/from iram channel using 1 parameter channel
 *
 * Playback
 * ram copy channel kicks off first,
 * 1st ram copy of entire iram buffer completion kicks off asp channel
 * asp tcc always kicks off ram copy of 1/2 iram buffer
 *
 * Record
 * asp channel starts, tcc kicks off ram copy
 */
static int request_ping_pong(struct snd_pcm_substream *substream,
		struct davinci_runtime_data *prtd,
		struct snd_dma_buffer *iram_dma)
{
	dma_addr_t asp_src_ping;
	dma_addr_t asp_dst_ping;
	int ret;
	struct davinci_pcm_dma_params *params = prtd->params;

	/* Request ram master channel */
	ret = prtd->ram_channel = edma_alloc_channel(EDMA_CHANNEL_ANY,
				  davinci_pcm_dma_irq, substream,
				  prtd->params->ram_chan_q);
	if (ret < 0)
		goto exit1;

	/* Request ram link channel */
	ret = prtd->ram_link = edma_alloc_slot(
			EDMA_CTLR(prtd->ram_channel), EDMA_SLOT_ANY);
	if (ret < 0)
		goto exit2;

	ret = prtd->asp_link[1] = edma_alloc_slot(
			EDMA_CTLR(prtd->asp_channel), EDMA_SLOT_ANY);
	if (ret < 0)
		goto exit3;

	prtd->ram_link2 = -1;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = prtd->ram_link2 = edma_alloc_slot(
			EDMA_CTLR(prtd->ram_channel), EDMA_SLOT_ANY);
		if (ret < 0)
			goto exit4;
	}
	/* circle ping-pong buffers */
	edma_link(prtd->asp_link[0], prtd->asp_link[1]);
	edma_link(prtd->asp_link[1], prtd->asp_link[0]);
	/* circle ram buffers */
	edma_link(prtd->ram_link, prtd->ram_link);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		asp_src_ping = iram_dma->addr;
		asp_dst_ping = params->dma_addr;	/* fifo */
	} else {
		asp_src_ping = params->dma_addr;	/* fifo */
		asp_dst_ping = iram_dma->addr;
	}
	/* ping */
	edma_set_src(prtd->asp_link[0], asp_src_ping, INCR, W16BIT);
	edma_set_dest(prtd->asp_link[0], asp_dst_ping, INCR, W16BIT);
	edma_set_src_index(prtd->asp_link[0], 0, 0);
	edma_set_dest_index(prtd->asp_link[0], 0, 0);

	edma_read_slot(prtd->asp_link[0], &prtd->asp_params);
	prtd->asp_params.opt &= ~(TCCMODE | EDMA_TCC(0x3f) | TCINTEN);
	prtd->asp_params.opt |= TCCHEN |
		EDMA_TCC(prtd->ram_channel & 0x3f);
	edma_write_slot(prtd->asp_link[0], &prtd->asp_params);

	/* pong */
	edma_set_src(prtd->asp_link[1], asp_src_ping, INCR, W16BIT);
	edma_set_dest(prtd->asp_link[1], asp_dst_ping, INCR, W16BIT);
	edma_set_src_index(prtd->asp_link[1], 0, 0);
	edma_set_dest_index(prtd->asp_link[1], 0, 0);

	edma_read_slot(prtd->asp_link[1], &prtd->asp_params);
	prtd->asp_params.opt &= ~(TCCMODE | EDMA_TCC(0x3f));
	/* interrupt after every pong completion */
	prtd->asp_params.opt |= TCINTEN | TCCHEN |
		EDMA_TCC(prtd->ram_channel & 0x3f);
	edma_write_slot(prtd->asp_link[1], &prtd->asp_params);

	/* ram */
	edma_set_src(prtd->ram_link, iram_dma->addr, INCR, W32BIT);
	edma_set_dest(prtd->ram_link, iram_dma->addr, INCR, W32BIT);
	pr_debug("%s: audio dma channels/slots in use for ram:%u %u %u,"
		"for asp:%u %u %u\n", __func__,
		prtd->ram_channel, prtd->ram_link, prtd->ram_link2,
		prtd->asp_channel, prtd->asp_link[0],
		prtd->asp_link[1]);
	return 0;
exit4:
	edma_free_channel(prtd->asp_link[1]);
	prtd->asp_link[1] = -1;
exit3:
	edma_free_channel(prtd->ram_link);
	prtd->ram_link = -1;
exit2:
	edma_free_channel(prtd->ram_channel);
	prtd->ram_channel = -1;
exit1:
	return ret;
}

static int davinci_pcm_dma_request(struct snd_pcm_substream *substream)
{
	struct snd_dma_buffer *iram_dma;
	struct davinci_runtime_data *prtd = substream->runtime->private_data;
	struct davinci_pcm_dma_params *params = prtd->params;
	int ret;

	if (!params)
		return -ENODEV;

	/* Request asp master DMA channel */
	ret = prtd->asp_channel = edma_alloc_channel(params->channel,
			davinci_pcm_dma_irq, substream,
			prtd->params->asp_chan_q);
	if (ret < 0)
		goto exit1;

	/* Request asp link channels */
	ret = prtd->asp_link[0] = edma_alloc_slot(
			EDMA_CTLR(prtd->asp_channel), EDMA_SLOT_ANY);
	if (ret < 0)
		goto exit2;

	iram_dma = (struct snd_dma_buffer *)substream->dma_buffer.private_data;
	if (iram_dma) {
		if (request_ping_pong(substream, prtd, iram_dma) == 0)
			return 0;
		printk(KERN_WARNING "%s: dma channel allocation failed,"
				"not using sram\n", __func__);
	}

	/* Issue transfer completion IRQ when the channel completes a
	 * transfer, then always reload from the same slot (by a kind
	 * of loopback link).  The completion IRQ handler will update
	 * the reload slot with a new buffer.
	 *
	 * REVISIT save p_ram here after setting up everything except
	 * the buffer and its length (ccnt) ... use it as a template
	 * so davinci_pcm_enqueue_dma() takes less time in IRQ.
	 */
	edma_read_slot(prtd->asp_link[0], &prtd->asp_params);
	prtd->asp_params.opt |= TCINTEN |
		EDMA_TCC(EDMA_CHAN_SLOT(prtd->asp_channel));
	prtd->asp_params.link_bcntrld = EDMA_CHAN_SLOT(prtd->asp_link[0]) << 5;
	edma_write_slot(prtd->asp_link[0], &prtd->asp_params);
	return 0;
exit2:
	edma_free_channel(prtd->asp_channel);
	prtd->asp_channel = -1;
exit1:
	return ret;
}

static int davinci_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct davinci_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		edma_start(prtd->asp_channel);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
		    prtd->ram_channel >= 0) {
			/* copy 1st iram buffer */
			edma_start(prtd->ram_channel);
		}
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		edma_resume(prtd->asp_channel);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		edma_pause(prtd->asp_channel);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&prtd->lock);

	return ret;
}

static int davinci_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct davinci_runtime_data *prtd = substream->runtime->private_data;

	davinci_pcm_period_reset(substream);
	if (prtd->ram_channel >= 0) {
		int ret = ping_pong_dma_setup(substream);
		if (ret < 0)
			return ret;

		edma_write_slot(prtd->ram_channel, &prtd->ram_params);
		edma_write_slot(prtd->asp_channel, &prtd->asp_params);

		print_buf_info(prtd->ram_channel, "ram_channel");
		print_buf_info(prtd->ram_link, "ram_link");
		print_buf_info(prtd->ram_link2, "ram_link2");
		print_buf_info(prtd->asp_channel, "asp_channel");
		print_buf_info(prtd->asp_link[0], "asp_link[0]");
		print_buf_info(prtd->asp_link[1], "asp_link[1]");

		/*
		 * There is a phase offset of 2 periods between the position
		 * used by dma setup and the position reported in the pointer
		 * function.
		 *
		 * The phase offset, when not using ping-pong buffers, is due to
		 * the two consecutive calls to davinci_pcm_enqueue_dma() below.
		 *
		 * Whereas here, with ping-pong buffers, the phase is due to
		 * there being an entire buffer transfer complete before the
		 * first dma completion event triggers davinci_pcm_dma_irq().
		 */
		davinci_pcm_period_elapsed(substream);
		davinci_pcm_period_elapsed(substream);

		return 0;
	}
	davinci_pcm_enqueue_dma(substream);
	davinci_pcm_period_elapsed(substream);

	/* Copy self-linked parameter RAM entry into master channel */
	edma_read_slot(prtd->asp_link[0], &prtd->asp_params);
	edma_write_slot(prtd->asp_channel, &prtd->asp_params);
	davinci_pcm_enqueue_dma(substream);
	davinci_pcm_period_elapsed(substream);

	return 0;
}

static snd_pcm_uframes_t
davinci_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct davinci_runtime_data *prtd = runtime->private_data;
	unsigned int offset;
	int asp_count;
	unsigned int period_size = snd_pcm_lib_period_bytes(substream);

	/*
	 * There is a phase offset of 2 periods between the position used by dma
	 * setup and the position reported in the pointer function. Either +2 in
	 * the dma setup or -2 here in the pointer function (with wrapping,
	 * both) accounts for this offset -- choose the latter since it makes
	 * the first-time setup clearer.
	 */
	spin_lock(&prtd->lock);
	asp_count = prtd->period - 2;
	spin_unlock(&prtd->lock);

	if (asp_count < 0)
		asp_count += runtime->periods;
	asp_count *= period_size;

	offset = bytes_to_frames(runtime, asp_count);
	if (offset >= runtime->buffer_size)
		offset = 0;

	return offset;
}

static int davinci_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct davinci_runtime_data *prtd;
	struct snd_pcm_hardware *ppcm;
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct davinci_pcm_dma_params *pa;
	struct davinci_pcm_dma_params *params;

	pa = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	if (!pa)
		return -ENODEV;
	params = &pa[substream->stream];

	ppcm = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
			&pcm_hardware_playback : &pcm_hardware_capture;
	allocate_sram(substream, params->sram_pool, params->sram_size, ppcm);
	snd_soc_set_runtime_hwparams(substream, ppcm);
	/* ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;

	prtd = kzalloc(sizeof(struct davinci_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);
	prtd->params = params;
	prtd->asp_channel = -1;
	prtd->asp_link[0] = prtd->asp_link[1] = -1;
	prtd->ram_channel = -1;
	prtd->ram_link = -1;
	prtd->ram_link2 = -1;

	runtime->private_data = prtd;

	ret = davinci_pcm_dma_request(substream);
	if (ret) {
		printk(KERN_ERR "davinci_pcm: Failed to get dma channels\n");
		kfree(prtd);
	}

	return ret;
}

static int davinci_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct davinci_runtime_data *prtd = runtime->private_data;

	if (prtd->ram_channel >= 0)
		edma_stop(prtd->ram_channel);
	if (prtd->asp_channel >= 0)
		edma_stop(prtd->asp_channel);
	if (prtd->asp_link[0] >= 0)
		edma_unlink(prtd->asp_link[0]);
	if (prtd->asp_link[1] >= 0)
		edma_unlink(prtd->asp_link[1]);
	if (prtd->ram_link >= 0)
		edma_unlink(prtd->ram_link);

	if (prtd->asp_link[0] >= 0)
		edma_free_slot(prtd->asp_link[0]);
	if (prtd->asp_link[1] >= 0)
		edma_free_slot(prtd->asp_link[1]);
	if (prtd->asp_channel >= 0)
		edma_free_channel(prtd->asp_channel);
	if (prtd->ram_link >= 0)
		edma_free_slot(prtd->ram_link);
	if (prtd->ram_link2 >= 0)
		edma_free_slot(prtd->ram_link2);
	if (prtd->ram_channel >= 0)
		edma_free_channel(prtd->ram_channel);

	kfree(prtd);

	return 0;
}

int davinci_soc_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &davinci_soc_platform);
}
EXPORT_SYMBOL_GPL(davinci_soc_platform_register);

void davinci_soc_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(davinci_soc_platform_unregister);

MODULE_AUTHOR("Vladimir Barinov");
MODULE_DESCRIPTION("TI DAVINCI PCM DMA module");
MODULE_LICENSE("GPL");
