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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/pci.h>

#include "ata_blacklist.h"

pcibl_t	sol11ata_pciide_blacklist[] = {
	/*
	 * The Nat SEMI PC87415 doesn't handle data and status byte
	 * synchornization correctly if an I/O error occurs that
	 * stops the request before the last sector.  I think it can
	 * cause lockups. See section 7.4.5.3 of the PC87415 spec.
	 * It's also rumored to be a "single fifo" type chip that can't
	 * DMA on both channels correctly.
	 */
	{ 0x100b, 0xffff, 0x2, 0xffff, ATA_BL_BOGUS},

	/*
	 * The CMD chip 0x646 does not support the use of interrupt bit
	 * in the busmaster ide status register when PIO is used.
	 * DMA is explicitly disabled for this legacy chip
	 */
	{ 0x1095, 0xffff, 0x0646, 0xffff, ATA_BL_BMSTATREG_PIO_BROKEN |
							ATA_BL_NODMA},

	/*
	 * Ditto for Serverworks CSB5 and CSB6 chips, but we can
	 * handle DMA.  Also, when emulating OSB4 mode, the simplex
	 * bit lies!
	 */
	{ 0x1166, 0xffff, 0x0212, 0xffff, ATA_BL_BMSTATREG_PIO_BROKEN|
							ATA_BL_NO_SIMPLEX},
	{ 0x1166, 0xffff, 0x0213, 0xffff, ATA_BL_BMSTATREG_PIO_BROKEN},

	{ 0, 0, 0, 0, 0 }
};

/*
 * add drives that have DMA or other problems to this list
 */

atabl_t	sol11ata_drive_blacklist[] = {
	{ "NEC CD-ROM DRIVE:260",	ATA_BL_1SECTOR },
	{ "NEC CD-ROM DRIVE:272",	ATA_BL_1SECTOR },
	{ "NEC CD-ROM DRIVE:273",	ATA_BL_1SECTOR },

	{ /* Mitsumi */ "FX001DE",	ATA_BL_1SECTOR },

	{ "fubar",
		(ATA_BL_NODMA |
		ATA_BL_1SECTOR |
		ATA_BL_NORVRT |
		ATA_BL_BOGUS |
		ATA_BL_BMSTATREG_PIO_BROKEN)
	},
	NULL
};
