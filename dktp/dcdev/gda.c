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

#include <sys/scsi/scsi.h>

#include <dktp/dadev.h>
#include <dktp/gda.h>

/*
 *	Generic Direct Attached Device
 */

static char *sol11gda_name(uchar_t cmd, char **cmdvec);

#ifdef	GDA_DEBUG
#define	DENT	0x0001
#define	DPKT	0x0002
#define	DERR	0x0004
static	int	sol11gda_debug = DERR|DENT|DPKT;

#endif	/* GDA_DEBUG */

/*
 * Local static data
 */

/*
 *	global data
 */

/*
 *	This is the loadable module wrapper
 */
#include <sys/modctl.h>

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops,	/* Type of module */
	"Solaris 11 Generic Direct Attached Device Utilities"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
#ifdef GDA_DEBUG
	if (sol11gda_debug & DENT)
		PRF("sol11gda_fini: call\n");
#endif
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


void
sol11gda_inqfill(char *p, int l, char *s)
{
	register unsigned i = 0, c;

	if (!p)
		return;
	while (i++ < l) {
/* 		clean strings of non-printing chars			*/
		if ((c = *p++) < ' ' || c > 0176) {
			c = ' ';
		}
		*s++ = (char)c;
	}
	*s++ = 0;
}

static char *
sol11gda_name(uchar_t cmd, char **cmdvec)
{
	while (*cmdvec != NULL) {
		if (cmd == **cmdvec) {
			return (*cmdvec + 1);
		}
		cmdvec++;
	}
	return ("<undecoded cmd>");
}


struct cmpkt *
sol11gda_pktprep(opaque_t objp, struct cmpkt *in_pktp, opaque_t dmatoken,
	int (*callback)(caddr_t), caddr_t arg)
{
	register struct	cmpkt *pktp;
	register struct	buf *bp = (struct buf *)dmatoken;

	if (in_pktp) {
		pktp = in_pktp;
	} else {
		pktp = CTL_PKTALLOC(objp, callback, arg);
		if (pktp == NULL)
			return (NULL);
	}

	if (bp) {
		if (bp->b_bcount) {
			if (CTL_MEMSETUP(objp, pktp, bp, callback, arg) ==
			    NULL) {
				if (!in_pktp)
					CTL_PKTFREE(objp, pktp);
				return (NULL);
			}
		}
		bp->av_back = (struct buf *)pktp;
		pktp->cp_bp = bp;
	}
	pktp->cp_retry = 0;
	pktp->cp_objp  = objp;


#ifdef GDA_DEBUG
	if (sol11gda_debug & DPKT)
		PRF("sol11gda_pktprep: pktp=0x%x \n", pktp);
#endif
	return (pktp);
}

void
sol11gda_free(opaque_t objp, struct cmpkt *pktp, struct buf *bp)
{
	if (pktp) {
		CTL_MEMFREE(objp, pktp);
		CTL_PKTFREE(objp, pktp);
	}

	if (bp) {
		if (bp->b_un.b_addr)
			i_ddi_mem_free((caddr_t)bp->b_un.b_addr, 0);
		freerbuf(bp);
	}
}

void
sol11gda_log(dev_info_t *dev, char *label, uint_t level, const char *fmt, ...)
{
	auto char name[256];
	auto char buf [256];
	va_list ap;
	int log_only = 0;
	int boot_only = 0;
	int console_only = 0;

	switch (*fmt) {
	case '!':
		log_only = 1;
		fmt++;
		break;
	case '?':
		boot_only = 1;
		fmt++;
		break;
	case '^':
		console_only = 1;
		fmt++;
		break;
	}


	if (dev) {
		if (level == CE_PANIC || level == CE_WARN) {
			(void) sprintf(name, "%s (%s%d):\n",
				ddi_pathname(dev, buf), label,
				ddi_get_instance(dev));
		} else if (level == CE_NOTE ||
		    level >= (uint_t)SCSI_DEBUG) {
			(void) sprintf(name,
			    "%s%d:", label, ddi_get_instance(dev));
		} else if (level == CE_CONT) {
			name[0] = '\0';
		}
	} else {
		(void) sprintf(name, "%s:", label);
	}

	va_start(ap, fmt);
	(void) vsprintf(buf, fmt, ap);
	va_end(ap);

	switch (level) {
		case CE_NOTE:
			level = CE_CONT;
			/* FALLTHROUGH */
		case CE_CONT:
		case CE_WARN:
		case CE_PANIC:
			if (boot_only) {
				cmn_err(level, "?%s\t%s", name, buf);
			} else if (console_only) {
				cmn_err(level, "^%s\t%s", name, buf);
			} else if (log_only) {
				cmn_err(level, "!%s\t%s", name, buf);
			} else {
				cmn_err(level, "%s\t%s", name, buf);
			}
			break;
		default:
			cmn_err(CE_CONT, "^DEBUG: %s\t%s", name, buf);
			break;
	}
}

void
sol11gda_errmsg(struct scsi_device *devp, struct cmpkt *pktp, char *label,
    int severity, int blkno, int err_blkno,
    char **cmdvec, char **senvec)
{
	auto char buf[256];
	dev_info_t *dev = devp->sd_dev;
	static char *error_classes[] = {
		"All", "Unknown", "Informational",
		"Recovered", "Retryable", "Fatal"
	};

	bzero((caddr_t)buf, 256);
	(void) sprintf(buf, "Error for command '%s'\tError Level: %s",
		sol11gda_name(*(uchar_t *)pktp->cp_cdbp, cmdvec),
		error_classes[severity]);
	sol11gda_log(dev, label, CE_WARN, buf);

	bzero((caddr_t)buf, 256);
	if ((blkno != -1) && (err_blkno != -1)) {
		(void) sprintf(buf, "Requested Block %d, Error Block: %d\n",
		    blkno, err_blkno);
		sol11gda_log(dev, label, CE_CONT, buf);
	}

	bzero((caddr_t)buf, 256);
	(void) sprintf(buf, "Sense Key: %s\n",
		sol11gda_name(*(uchar_t *)pktp->cp_scbp, senvec));

	sol11gda_log(dev, label, CE_CONT, buf);
	bzero((caddr_t)buf, 256);
	(void) strcpy(buf, "Vendor '");
	sol11gda_inqfill(devp->sd_inq->inq_vid, 8, &buf[strlen(buf)]);
	(void) sprintf(&buf[strlen(buf)],
		"' error code: 0x%x",
		*(uchar_t *)pktp->cp_scbp);
	sol11gda_log(dev, label, CE_CONT, "%s\n", buf);
}
