/*
 * drivers/video/tegra/host/t20/channel_t20.c
 *
 * Tegra Graphics Host Channel
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "../nvhost_channel.h"
#include "../dev.h"
#include "../nvhost_hwctx.h"
#include <trace/events/nvhost.h>

#include "hardware_t20.h"
#include "syncpt_t20.h"

#define NVHOST_NUMCHANNELS (NV_HOST1X_CHANNELS - 1)
#define NVHOST_CHANNEL_BASE 0

#define NVMODMUTEX_2D_FULL   (1)
#define NVMODMUTEX_2D_SIMPLE (2)
#define NVMODMUTEX_2D_SB_A   (3)
#define NVMODMUTEX_2D_SB_B   (4)
#define NVMODMUTEX_3D        (5)
#define NVMODMUTEX_DISPLAYA  (6)
#define NVMODMUTEX_DISPLAYB  (7)
#define NVMODMUTEX_VI        (8)
#define NVMODMUTEX_DSI       (9)

static void power_2d(struct nvhost_module *mod, enum nvhost_power_action action);
static void power_3d(struct nvhost_module *mod, enum nvhost_power_action action);
static void power_mpe(struct nvhost_module *mod, enum nvhost_power_action action);



static const struct nvhost_channeldesc channelmap[] = {
{
	/* channel 0 */
	.name	       = "display",
	.syncpts       = BIT(NVSYNCPT_DISP0) | BIT(NVSYNCPT_DISP1) |
			 BIT(NVSYNCPT_VBLANK0) | BIT(NVSYNCPT_VBLANK1),
	.modulemutexes = BIT(NVMODMUTEX_DISPLAYA) | BIT(NVMODMUTEX_DISPLAYB),
},
{
	/* channel 1 */
	.name	       = "gr3d",
	.syncpts       = BIT(NVSYNCPT_3D),
	.waitbases     = BIT(NVWAITBASE_3D),
	.modulemutexes = BIT(NVMODMUTEX_3D),
	.class	       = NV_GRAPHICS_3D_CLASS_ID,
	.power         = power_3d,
},
{
	/* channel 2 */
	.name	       = "gr2d",
	.syncpts       = BIT(NVSYNCPT_2D_0) | BIT(NVSYNCPT_2D_1),
	.waitbases     = BIT(NVWAITBASE_2D_0) | BIT(NVWAITBASE_2D_1),
	.modulemutexes = BIT(NVMODMUTEX_2D_FULL) | BIT(NVMODMUTEX_2D_SIMPLE) |
			 BIT(NVMODMUTEX_2D_SB_A) | BIT(NVMODMUTEX_2D_SB_B),
	.power         = power_2d,
},
{
	/* channel 3 */
	.name	 = "isp",
	.syncpts = 0,
},
{
	/* channel 4 */
	.name	       = "vi",
	.syncpts       = BIT(NVSYNCPT_CSI_VI_0) | BIT(NVSYNCPT_CSI_VI_1) |
			 BIT(NVSYNCPT_VI_ISP_0) | BIT(NVSYNCPT_VI_ISP_1) |
			 BIT(NVSYNCPT_VI_ISP_2) | BIT(NVSYNCPT_VI_ISP_3) |
			 BIT(NVSYNCPT_VI_ISP_4),
	.modulemutexes = BIT(NVMODMUTEX_VI),
	.exclusive     = true,
},
{
	/* channel 5 */
	.name	       = "mpe",
	.syncpts       = BIT(NVSYNCPT_MPE) | BIT(NVSYNCPT_MPE_EBM_EOF) |
			 BIT(NVSYNCPT_MPE_WR_SAFE),
	.waitbases     = BIT(NVWAITBASE_MPE),
	.class	       = NV_VIDEO_ENCODE_MPEG_CLASS_ID,
	.power	       = power_mpe,
	.exclusive     = true,
	.keepalive     = true,
},
{
	/* channel 6 */
	.name	       = "dsi",
	.syncpts       = BIT(NVSYNCPT_DSI),
	.modulemutexes = BIT(NVMODMUTEX_DSI),
}};

static inline void __iomem *t20_channel_aperture(void __iomem *p, int ndx)
{
	ndx += NVHOST_CHANNEL_BASE;
	p += NV_HOST1X_CHANNEL0_BASE;
	p += ndx * NV_HOST1X_CHANNEL_MAP_SIZE_BYTES;
	return p;
}


int t20_nvhost_3dctx_handler_init(struct nvhost_hwctx_handler *h);
int t20_nvhost_mpectx_handler_init(struct nvhost_hwctx_handler *h);

static inline int t20_nvhost_hwctx_handler_init(
	struct nvhost_hwctx_handler *h,
	const char *module)
{
	if (strcmp(module, "gr3d") == 0)
		return t20_nvhost_3dctx_handler_init(h);
	else if (strcmp(module, "mpe") == 0)
		return t20_nvhost_mpectx_handler_init(h);

	return 0;
}


static int t20_channel_init(struct nvhost_channel *ch,
			    struct nvhost_master *dev, int index)
{
	ch->dev = dev;
	ch->desc = channelmap + index;
	mutex_init(&ch->reflock);
	mutex_init(&ch->submitlock);

	ch->aperture = t20_channel_aperture(dev->aperture, index);

	return t20_nvhost_hwctx_handler_init(&ch->ctxhandler, ch->desc->name);
}



static int t20_channel_submit(struct nvhost_channel *channel,
			      struct nvhost_hwctx *hwctx,
			      struct nvmap_client *user_nvmap,
			      u32 *gather,
			      u32 *gather_end,
			      struct nvhost_waitchk *waitchk,
			      struct nvhost_waitchk *waitchk_end,
			      u32 waitchk_mask,
			      struct nvmap_handle **unpins,
			      int nr_unpins,
			      u32 syncpt_id,
			      u32 syncpt_incrs,
			      u32 *syncpt_value,
			      bool null_kickoff)
{
	struct nvhost_hwctx *hwctx_to_save = NULL;
	struct nvhost_syncpt *sp = &channel->dev->syncpt;
	u32 user_syncpt_incrs = syncpt_incrs;
	bool need_restore = false;
	u32 syncval;
	int err;

	/* keep module powered */
	nvhost_module_busy(&channel->mod);

	/* get submit lock */
	err = mutex_lock_interruptible(&channel->submitlock);
	if (err) {
		nvhost_module_idle(&channel->mod);
		return err;
	}

	/* remove stale waits */
	if (waitchk != waitchk_end) {
		err = nvhost_syncpt_wait_check(sp,
					       user_nvmap,
					       waitchk_mask,
					       waitchk, waitchk_end);
		if (err) {
			dev_warn(&channel->dev->pdev->dev,
				 "nvhost_syncpt_wait_check failed: %d\n", err);
			mutex_unlock(&channel->submitlock);
			nvhost_module_idle(&channel->mod);
			return err;
		}
	}

	/* context switch */
	if (channel->cur_ctx != hwctx) {
		trace_nvhost_channel_context_switch(channel->desc->name,
		  channel->cur_ctx, hwctx);
		hwctx_to_save = channel->cur_ctx;
		if (hwctx_to_save) {
			syncpt_incrs += hwctx_to_save->save_incrs;
			hwctx_to_save->valid = true;
			channel->ctxhandler.get(hwctx_to_save);
		}
		channel->cur_ctx = hwctx;
		if (channel->cur_ctx && channel->cur_ctx->valid) {
			need_restore = true;
			syncpt_incrs += channel->cur_ctx->restore_incrs;
		}
	}

	/* get absolute sync value */
	if (BIT(syncpt_id) & sp->client_managed)
		syncval = nvhost_syncpt_set_max(sp,
						syncpt_id, syncpt_incrs);
	else
		syncval = nvhost_syncpt_incr_max(sp,
						syncpt_id, syncpt_incrs);

	/* begin a CDMA submit */
	nvhost_cdma_begin(&channel->cdma);

	/* push save buffer (pre-gather setup depends on unit) */
	if (hwctx_to_save)
		channel->ctxhandler.save_push(&channel->cdma, hwctx_to_save);

	/* gather restore buffer */
	if (need_restore)
		nvhost_cdma_push(&channel->cdma,
			nvhost_opcode_gather(channel->cur_ctx->restore_size),
			channel->cur_ctx->restore_phys);

	/* add a setclass for modules that require it (unless ctxsw added it) */
	if (!hwctx_to_save && !need_restore && channel->desc->class)
		nvhost_cdma_push(&channel->cdma,
			nvhost_opcode_setclass(channel->desc->class, 0, 0),
			NVHOST_OPCODE_NOOP);

	if (null_kickoff) {
		int incr;
		u32 op_incr;

		/* TODO ideally we'd also perform host waits here */

		/* push increments that correspond to nulled out commands */
		op_incr = nvhost_opcode_imm(0, 0x100 | syncpt_id);
		for (incr = 0; incr < (user_syncpt_incrs >> 1); incr++)
			nvhost_cdma_push(&channel->cdma, op_incr, op_incr);
		if (user_syncpt_incrs & 1)
			nvhost_cdma_push(&channel->cdma,
					op_incr, NVHOST_OPCODE_NOOP);

		/* for 3d, waitbase needs to be incremented after each submit */
		if (channel->desc->class == NV_GRAPHICS_3D_CLASS_ID)
			nvhost_cdma_push(&channel->cdma,
					nvhost_opcode_setclass(
						NV_HOST1X_CLASS_ID,
						NV_CLASS_HOST_INCR_SYNCPT_BASE,
						1),
					nvhost_class_host_incr_syncpt_base(
						NVWAITBASE_3D,
						user_syncpt_incrs));
	}
	else {
		/* push user gathers */
		for ( ; gather != gather_end; gather += 2)
			nvhost_cdma_push(&channel->cdma,
					nvhost_opcode_gather(gather[0]),
					gather[1]);
	}

	/* end CDMA submit & stash pinned hMems into sync queue */
	nvhost_cdma_end(&channel->cdma, user_nvmap,
			syncpt_id, syncval, unpins, nr_unpins);

	/*
	 * schedule a context save interrupt (to drain the host FIFO
	 * if necessary, and to release the restore buffer)
	 */
	if (hwctx_to_save)
		nvhost_intr_add_action(&channel->dev->intr, syncpt_id,
			syncval - syncpt_incrs + hwctx_to_save->save_thresh,
			NVHOST_INTR_ACTION_CTXSAVE, hwctx_to_save, NULL);

	/* schedule a submit complete interrupt */
	nvhost_intr_add_action(&channel->dev->intr, syncpt_id, syncval,
			NVHOST_INTR_ACTION_SUBMIT_COMPLETE, channel, NULL);

	mutex_unlock(&channel->submitlock);

	*syncpt_value = syncval;
	return 0;
}

static void power_2d(struct nvhost_module *mod, enum nvhost_power_action action)
{
	/* TODO: [ahatala 2010-06-17] reimplement EPP hang war */
	if (action == NVHOST_POWER_ACTION_OFF) {
		/* TODO: [ahatala 2010-06-17] reset EPP */
	}
}

static void power_3d(struct nvhost_module *mod, enum nvhost_power_action action)
{
	struct nvhost_channel *ch = container_of(mod, struct nvhost_channel, mod);
	struct nvhost_hwctx *hwctx_to_save;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	u32 syncpt_incrs, syncpt_val;
	void *ref;

	if (action != NVHOST_POWER_ACTION_OFF)
		return;

	mutex_lock(&ch->submitlock);
	hwctx_to_save = ch->cur_ctx;
	if (!hwctx_to_save) {
		mutex_unlock(&ch->submitlock);
		return;
	}

	hwctx_to_save->valid = true;
	ch->ctxhandler.get(hwctx_to_save);
	ch->cur_ctx = NULL;

	syncpt_incrs = hwctx_to_save->save_incrs;
	syncpt_val = nvhost_syncpt_incr_max(&ch->dev->syncpt,
					NVSYNCPT_3D, syncpt_incrs);

	nvhost_cdma_begin(&ch->cdma);
	ch->ctxhandler.save_push(&ch->cdma, hwctx_to_save);
	nvhost_cdma_end(&ch->cdma, ch->dev->nvmap, NVSYNCPT_3D, syncpt_val, NULL, 0);

	nvhost_intr_add_action(&ch->dev->intr, NVSYNCPT_3D,
			syncpt_val - syncpt_incrs + hwctx_to_save->save_thresh,
			NVHOST_INTR_ACTION_CTXSAVE, hwctx_to_save, NULL);

	nvhost_intr_add_action(&ch->dev->intr, NVSYNCPT_3D, syncpt_val,
			NVHOST_INTR_ACTION_WAKEUP, &wq, &ref);
	wait_event(wq,
		nvhost_syncpt_min_cmp(&ch->dev->syncpt,
				NVSYNCPT_3D, syncpt_val));

	nvhost_intr_put_ref(&ch->dev->intr, ref);

	nvhost_cdma_update(&ch->cdma);

	mutex_unlock(&ch->submitlock);
}

static void power_mpe(struct nvhost_module *mod, enum nvhost_power_action action)
{
}

int nvhost_init_t20_channel_support(struct nvhost_master *host)
{

	BUILD_BUG_ON(NVHOST_NUMCHANNELS != ARRAY_SIZE(channelmap));

	host->nb_mlocks =  NV_HOST1X_SYNC_MLOCK_NUM;
	host->nb_channels =  NVHOST_NUMCHANNELS;

	host->op.channel.init = t20_channel_init;
	host->op.channel.submit = t20_channel_submit;

	return 0;
}
