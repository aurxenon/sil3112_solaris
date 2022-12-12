/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").  
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/debug.h>

#include "ata_common.h"
#include "ata_disk.h"
#include "atapi.h"
#include "pciide.h"


#ifdef ATA_DEBUG

void
dump_sol11ata_ctl(sol11ata_ctl_t *P)
{
	sol11ghd_err("dip 0x%p flags 0x%x timing 0x%x\n",
		P->ac_dip, P->ac_flags, P->ac_timing_flags);
	sol11ghd_err("drvp[0][0..7] 0x%p 0x%p 0x%p 0x%p 0x%p 0x%p 0x%p 0x%p\n",
		P->ac_drvp[0][0], P->ac_drvp[0][1], P->ac_drvp[0][2],
		P->ac_drvp[0][3], P->ac_drvp[0][4], P->ac_drvp[0][5],
		P->ac_drvp[0][6], P->ac_drvp[0][7]);
	sol11ghd_err("drvp[1][0..7] 0x%p 0x%p 0x%p 0x%p 0x%p 0x%p 0x%p 0x%p\n",
		P->ac_drvp[1][0], P->ac_drvp[1][1], P->ac_drvp[1][2],
		P->ac_drvp[1][3], P->ac_drvp[1][4], P->ac_drvp[1][5],
		P->ac_drvp[1][6], P->ac_drvp[1][7]);
	sol11ghd_err("max tran 0x%x &ccc_t 0x%p actv drvp 0x%p actv pktp 0x%p\n",
		P->ac_max_transfer, &P->ac_ccc,
		P->ac_active_drvp, P->ac_active_pktp);
	sol11ghd_err("state %d hba tranp 0x%p\n", P->ac_state, P->ac_sol11atapi_tran);
	sol11ghd_err("iohdl1 0x%p 0x%p D 0x%p E 0x%p F 0x%p C 0x%p S 0x%p LC 0x%p "
		"HC 0x%p HD 0x%p ST 0x%p CMD 0x%p\n",
		P->ac_iohandle1, P->ac_ioaddr1, P->ac_data, P->ac_error,
		P->ac_feature, P->ac_count, P->ac_sect, P->ac_lcyl,
		P->ac_hcyl, P->ac_drvhd, P->ac_status, P->ac_cmd);
	sol11ghd_err("iohdl2 0x%p 0x%p AST 0x%p DC 0x%p\n",
		P->ac_iohandle2, P->ac_ioaddr2, P->ac_altstatus, P->ac_devctl);
	sol11ghd_err("bm hdl 0x%p 0x%p pciide %d BM %d sg_list 0x%p paddr 0x%llx "
		"acc hdl 0x%p sg hdl 0x%p\n",
		P->ac_bmhandle, P->ac_bmaddr, P->ac_pciide, P->ac_pciide_bm,
		P->ac_sg_list, (unsigned long long) P->ac_sg_paddr,
		P->ac_sg_acc_handle, P->ac_sg_handle);
	sol11ghd_err("arq pktp 0x%p flt pktp 0x%p &cdb 0x%p\n",
		P->ac_arq_pktp, P->ac_fault_pktp, &P->ac_arq_cdb);
}

void
dump_sol11ata_drv(sol11ata_drv_t *P)
{


	sol11ghd_err("ctlp 0x%p &sol11ata_id 0x%p flags 0x%x pciide dma 0x%x\n",
		P->ad_ctlp, &P->ad_id, P->ad_flags, P->ad_pciide_dma);

	sol11ghd_err("targ %d lun %d driv 0x%x state %d cdb len %d "
		"bogus %d nec %d\n", P->ad_targ, P->ad_lun, P->ad_drive_bits,
		P->ad_state, P->ad_cdb_len, P->ad_bogus_drq,
		P->ad_nec_bad_status);

	sol11ghd_err("sol11ata &scsi_dev 0x%p &scsi_inquiry 0x%p &ctl_obj 0x%p\n",
		&P->ad_device, &P->ad_inquiry, &P->ad_ctl_obj);

	sol11ghd_err("sol11ata rd cmd 0x%x wr cmd 0x%x acyl 0x%x\n",
		P->ad_rd_cmd, P->ad_wr_cmd, P->ad_acyl);

	sol11ghd_err("sol11ata bios cyl %d hd %d sec %d  phs hd %d sec %d\n",
		P->ad_drvrcyl, P->ad_drvrhd, P->ad_drvrsec, P->ad_phhd,
		P->ad_phsec);

	sol11ghd_err("block factor %d bpb %d\n",
		P->ad_block_factor, P->ad_bytes_per_block);
}

void
dump_sol11ata_pkt(sol11ata_pkt_t *P)
{
	sol11ghd_err("gcmdp 0x%p flags 0x%x v_addr 0x%p dma %d\n",
		P->ap_gcmdp, P->ap_flags, P->ap_v_addr, P->ap_pciide_dma);
	sol11ghd_err("&sg_list 0x%p sg cnt 0x%x resid 0x%lx bcnt 0x%lx\n",
		P->ap_sg_list, P->ap_sg_cnt, P->ap_resid, P->ap_bcount);
	sol11ghd_err("sec 0x%x cnt 0x%x lc 0x%x hc 0x%x hd 0x%x cmd 0x%x\n",
		P->ap_sec, P->ap_count, P->ap_lwcyl, P->ap_hicyl,
		P->ap_hd, P->ap_cmd);
	sol11ghd_err("status 0x%x error 0x%x\n", P->ap_status, P->ap_error);
	sol11ghd_err("start 0x%p intr 0x%p complete 0x%p\n",
		P->ap_start, P->ap_intr, P->ap_complete);
	sol11ghd_err("sol11ata cdb 0x%x scb 0x%x bpb 0x%x wrt cnt 0x%x\n",
		P->ap_cdb, P->ap_scb, P->ap_bytes_per_block, P->ap_wrt_count);
	sol11ghd_err("sol11atapi cdbp 0x%p cdb len %d cdb pad %d\n",
		P->ap_cdbp, P->ap_cdb_len, P->ap_cdb_pad);
	sol11ghd_err("scbp 0x%p statuslen 0x%x\n", P->ap_scbp, P->ap_statuslen);
}

#endif
