// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) Rockchip Electronics Co., Ltd. */

#include <linux/io.h>

#include "aiisp.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"
#include "version.h"

#ifdef CONFIG_PROC_FS

static int rkaiisp_show(struct seq_file *p, void *v)
{
	struct rkaiisp_device *aidev = p->private;
	struct rkaiisp_hw_dev *hw_dev = aidev->hw_dev;
	unsigned long flags = 0;
	u32 frm_rate;
	int idx_buf_len;

	seq_printf(p, "%-18s Version:v%02x.%02x.%02x\n", aidev->v4l2_dev.name,
		   RKAIISP_DRIVER_VERSION >> 16,
		   (RKAIISP_DRIVER_VERSION & 0xff00) >> 8,
		   RKAIISP_DRIVER_VERSION & 0x00ff);

	seq_printf(p, "%-18s %d\n", "dev id", aidev->dev_id);
	seq_printf(p, "%-18s %d\n", "frame id", aidev->frame_id);
	seq_printf(p, "%-18s %d\n", "run idx", aidev->run_idx);
	frm_rate = aidev->frm_st - aidev->pre_frm_st;
	seq_printf(p, "%-18s %d\n", "frm rate", frm_rate / 1000 / 1000);
	seq_printf(p, "%-18s %d\n", "frm hdltime", aidev->frm_interval / 1000 / 1000);
	seq_printf(p, "%-18s %d\n", "frm_oversdtim_cnt", aidev->frm_oversdtim_cnt);
	seq_printf(p, "%-18s %d\n", "model mode", aidev->model_mode);
	seq_printf(p, "%-18s %d\n", "model runcnt", aidev->model_runcnt);
	seq_printf(p, "%-18s %d\n", "max runcnt", aidev->max_runcnt);
	seq_printf(p, "%-18s %d\n", "para size", aidev->para_size);
	seq_printf(p, "%-18s %d\n", "hw state", aidev->hwstate);
	seq_printf(p, "%-18s %d\n", "iir width", aidev->ispbuf.iir_width);
	seq_printf(p, "%-18s %d\n", "iir height", aidev->ispbuf.iir_height);

	spin_lock_irqsave(&hw_dev->hw_lock, flags);
	idx_buf_len = rkaiisp_get_idxbuf_len(aidev);
	spin_unlock_irqrestore(&hw_dev->hw_lock, flags);
	seq_printf(p, "%-18s %d\n", "idx buf len", idx_buf_len);

	// hw
	seq_printf(p, "%-18s %d\n", "hw: is_single", hw_dev->is_single);
	seq_printf(p, "%-18s %d\n", "hw: dev_num", hw_dev->dev_num);
	seq_printf(p, "%-18s %d\n", "hw: cur_dev_id", hw_dev->cur_dev_id);

	return 0;
}

static int rkaiisp_open(struct inode *inode, struct file *file)
{
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	struct rkaiisp_device *aidev = pde_data(inode);
#else
	struct rkaiisp_device *aidev = PDE_DATA(inode);
#endif

	return single_open(file, rkaiisp_show, aidev);
}

static const struct proc_ops rkaiisp_proc_ops = {
	.proc_open = rkaiisp_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int rkaiisp_proc_init(struct rkaiisp_device *aidev)
{
	aidev->procfs = proc_create_data(aidev->v4l2_dev.name, 0444, NULL, &rkaiisp_proc_ops, aidev);
	if (!aidev->procfs)
		return -EINVAL;
	return 0;
}

void rkaiisp_proc_cleanup(struct rkaiisp_device *aidev)
{
	if (aidev->procfs)
		remove_proc_entry(aidev->v4l2_dev.name, NULL);
	aidev->procfs = NULL;
}
#endif /* CONFIG_PROC_FS */
