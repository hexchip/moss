/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "resource_load.h"

#include <zephyr/types.h>
#include <zephyr/devicetree.h>

#include <listenai/app-res.h>

#include "comp_res_gcl.h"
#include "csk_malloc.h"

#define COMPONENTS_RESOURCE_ITEMS_MAX (16)

int components_resource_load(void)
{
	gcl_comp_res_mgr_ipc_t *res_mgr;
	uint32_t res_mgr_size;

	res_mgr_size = sizeof(gcl_comp_res_mgr_ipc_t) +
		       sizeof(gcl_comp_res_mgr_item_ipc_t) * COMPONENTS_RESOURCE_ITEMS_MAX;
	res_mgr = csk_malloc(res_mgr_size);
	res_mgr->number = 0;

#if CONFIG_CAPABILITY_FD
	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_FD_FACE_DETECT;
	res_mgr->item[res_mgr->number].res_addr = APP_RES_DT_ADDR(fd_face_detect);
	res_mgr->item[res_mgr->number].res_size = APP_RES_DT_SIZE(fd_face_detect);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_FD_FACE_ALIGN;
	res_mgr->item[res_mgr->number].res_addr = APP_RES_DT_ADDR(fd_face_align);
	res_mgr->item[res_mgr->number].res_size = APP_RES_DT_SIZE(fd_face_align);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_FD_FACE_VERIFY;
	res_mgr->item[res_mgr->number].res_addr = APP_RES_DT_ADDR(fd_face_verify);
	res_mgr->item[res_mgr->number].res_size = APP_RES_DT_SIZE(fd_face_verify);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_FD_ANTI_SPOOFING;
	res_mgr->item[res_mgr->number].res_addr = APP_RES_DT_ADDR(fd_anti_spoofing);
	res_mgr->item[res_mgr->number].res_size = APP_RES_DT_SIZE(fd_anti_spoofing);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_FD_FACE_DEPTH;
	res_mgr->item[res_mgr->number].res_addr = APP_RES_DT_ADDR(fd_face_depth);
	res_mgr->item[res_mgr->number].res_size = APP_RES_DT_SIZE(fd_face_depth);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_FD_ANTI_BILINEAR;
	res_mgr->item[res_mgr->number].res_addr = APP_RES_DT_ADDR(fd_anti_bilinear);
	res_mgr->item[res_mgr->number].res_size = APP_RES_DT_SIZE(fd_anti_bilinear);
	res_mgr->number++;
#endif

	gcl_comps_res_load(res_mgr);
	csk_free(res_mgr);

	return 0;
}
