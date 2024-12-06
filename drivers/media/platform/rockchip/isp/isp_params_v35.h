/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_ISP_PARAM_V35_H
#define _RKISP_ISP_PARAM_V35_H

#include "isp_params.h"

#define ISP35_RAWHISTBIG_ROW_NUM		15
#define ISP35_RAWHISTBIG_COLUMN_NUM		15
#define ISP35_RAWHISTBIG_WEIGHT_REG_SIZE	\
	(ISP35_RAWHISTBIG_ROW_NUM * ISP35_RAWHISTBIG_COLUMN_NUM)

struct rkisp_isp_params_vdev;
struct rkisp_isp_params_val_v35 {
	struct rkisp_dummy_buffer buf_ldch[ISP_UNITE_MAX][ISP3X_MESH_BUF_NUM];
	u32 buf_ldch_idx[ISP_UNITE_MAX];
	struct rkisp_dummy_buffer buf_b3dldc[ISP_UNITE_MAX][ISP3X_MESH_BUF_NUM];
	u32 buf_b3dldc_idx[ISP_UNITE_MAX];
	u32 b3dldc_hsize;
	u32 b3dldch_vsize;
	u32 b3dldcv_vsize;
	struct rkisp_dummy_buffer buf_info[RKISP_INFO2DDR_BUF_MAX];
	u32 buf_info_owner;
	u32 buf_info_cnt;
	int buf_info_idx;

	struct rkisp_dummy_buffer buf_aiawb[RKISP_BUFFER_MAX];
	u32 buf_aiawb_cnt;
	int buf_aiawb_idx;

	struct rkisp_dummy_buffer buf_3dnr_wgt;
	struct rkisp_dummy_buffer buf_3dnr_iir;
	struct rkisp_dummy_buffer buf_3dnr_ds;
	struct rkisp_dummy_buffer buf_gain;
	u32 bay3d_iir_stride;
	u32 bay3d_ds_size;
	u32 bay3d_iir_size;
	u32 bay3d_wgt_size;
	u32 gain_size;

	u32 hist_blk_num;
	u32 enh_row;
	u32 enh_col;
};

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35)
int rkisp_init_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev);
void rkisp_uninit_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev);
#else
static inline int rkisp_init_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev) { return -EINVAL; }
static inline void rkisp_uninit_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev) {}
#endif
#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35_DBG)
int rkisp_get_params_v35(struct rkisp_isp_params_vdev *params_vdev, void *arg);
#else
static inline int rkisp_get_params_v35(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	pr_err("enable CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35_DBG in kernel config\n");
	return -EINVAL;
}
#endif

#endif /* _RKISP_ISP_PARAM_V35_H */
