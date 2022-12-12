/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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

/*
 *	Device Strategy
 */
#include <dktp/cm.h>
#include <sys/kstat.h>

#include <dktp/quetypes.h>
#include <dktp/queue.h>
#include <dktp/tgcom.h>
#include <dktp/fctypes.h>
#include <dktp/flowctrl.h>
#include <sys/param.h>
#include <vm/page.h>
#include <sys/modctl.h>

/*
 *	Object Management
 */

static struct buf *sol11qmerge_nextbp(struct sol11que_data *qfp, struct buf *bp_merge,
    int *can_merge);

static struct modlmisc modlmisc = {
	&mod_miscops,	/* Type of module */
	"Solaris 11 Device Strategy Objects"
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlmisc,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 *	Common Flow Control functions
 */

/*
 * Local static data
 */
#ifdef	FLC_DEBUG
#define	DENT	0x0001
#define	DERR	0x0002
#define	DIO	0x0004
static	int	sol11flc_debug = DENT|DERR|DIO;

#include <sys/thread.h>
static 	int	sol11flc_malloc_intr = 0;
#endif	/* FLC_DEBUG */

static	int	sol11flc_kstat = 1;

static struct sol11flc_obj *sol11fc_create(struct sol11flc_objops *fcopsp);
static int sol11fc_init(opaque_t queuep, opaque_t tgcom_objp, opaque_t sol11que_objp,
    void *lkarg);
static int sol11fc_free(struct sol11flc_obj *flcobjp);
static int sol11fc_start_kstat(opaque_t queuep, char *devtype, int instance);
static int sol11fc_stop_kstat(opaque_t queuep);

static struct sol11flc_obj *
sol11fc_create(struct sol11flc_objops *fcopsp)
{
	struct	sol11flc_obj *flcobjp;
	struct	sol11fc_data *fcdp;

	flcobjp = kmem_zalloc((sizeof (*flcobjp) + sizeof (*fcdp)), KM_NOSLEEP);
	if (!flcobjp)
		return (NULL);

	fcdp = (struct sol11fc_data *)(flcobjp+1);
	flcobjp->sol11flc_data = (opaque_t)fcdp;
	flcobjp->sol11flc_ops  = fcopsp;

	return ((opaque_t)flcobjp);
}

static int sol11dmult_maxcnt = DMULT_MAXCNT;

static int
sol11fc_init(opaque_t queuep, opaque_t tgcom_objp, opaque_t sol11que_objp, void *lkarg)
{
	struct sol11fc_data *fcdp = (struct sol11fc_data *)queuep;

	mutex_init(&fcdp->ds_mutex, NULL, MUTEX_DRIVER, lkarg);

	fcdp->ds_queobjp   = sol11que_objp;
	fcdp->ds_tgcomobjp = tgcom_objp;
	fcdp->ds_waitcnt   = sol11dmult_maxcnt;

	SOL11QUE_INIT(sol11que_objp, lkarg);
	TGCOM_INIT(tgcom_objp);
	return (DDI_SUCCESS);
}

static int
sol11fc_free(struct sol11flc_obj *flcobjp)
{
	struct sol11fc_data *fcdp;

	fcdp = (struct sol11fc_data *)flcobjp->sol11flc_data;
	if (fcdp->ds_queobjp)
		SOL11QUE_FREE(fcdp->ds_queobjp);
	if (fcdp->ds_tgcomobjp) {
		TGCOM_FREE(fcdp->ds_tgcomobjp);
		mutex_destroy(&fcdp->ds_mutex);
	}
	kmem_free(flcobjp, (sizeof (*flcobjp) + sizeof (*fcdp)));
	return (0);
}

/*ARGSUSED*/
static int
sol11fc_start_kstat(opaque_t queuep, char *devtype, int instance)
{
	struct sol11fc_data *fcdp = (struct sol11fc_data *)queuep;
	if (!sol11flc_kstat)
		return (0);

	if (!fcdp->ds_kstat) {
		if (fcdp->ds_kstat = kstat_create("sol11cmdk", instance, NULL,
		    "disk", KSTAT_TYPE_IO, 1, KSTAT_FLAG_PERSISTENT)) {
			kstat_install(fcdp->ds_kstat);
		}
	}
	return (0);
}

static int
sol11fc_stop_kstat(opaque_t queuep)
{
	struct sol11fc_data *fcdp = (struct sol11fc_data *)queuep;

	if (fcdp->ds_kstat) {
		kstat_delete(fcdp->ds_kstat);
		fcdp->ds_kstat = NULL;
	}
	return (0);
}


/*
 *	Single Command per Device
 */
/*
 * Local Function Prototypes
 */
static int sol11dsngl_restart();

static int sol11dsngl_enque(opaque_t, struct buf *);
static int sol11dsngl_deque(opaque_t, struct buf *);

struct 	sol11flc_objops sol11dsngl_ops = {
	sol11fc_init,
	sol11fc_free,
	sol11dsngl_enque,
	sol11dsngl_deque,
	sol11fc_start_kstat,
	sol11fc_stop_kstat,
	0, 0
};

struct sol11flc_obj *
sol11dsngl_create()
{
	return (sol11fc_create((struct sol11flc_objops *)&sol11dsngl_ops));
}

static int
sol11dsngl_enque(opaque_t queuep, struct buf *in_bp)
{
	struct sol11fc_data *sol11dsnglp = (struct sol11fc_data *)queuep;
	opaque_t tgcom_objp;
	opaque_t sol11que_objp;

	sol11que_objp   = sol11dsnglp->ds_queobjp;
	tgcom_objp = sol11dsnglp->ds_tgcomobjp;

	if (!in_bp)
		return (0);
	mutex_enter(&sol11dsnglp->ds_mutex);
	if (sol11dsnglp->ds_bp || sol11dsnglp->ds_outcnt) {
		SOL11QUE_ADD(sol11que_objp, in_bp);
		if (sol11dsnglp->ds_kstat) {
			kstat_waitq_enter(KSTAT_IO_PTR(sol11dsnglp->ds_kstat));
		}
		mutex_exit(&sol11dsnglp->ds_mutex);
		return (0);
	}
	if (sol11dsnglp->ds_kstat) {
		kstat_waitq_enter(KSTAT_IO_PTR(sol11dsnglp->ds_kstat));
	}
	if (TGCOM_PKT(tgcom_objp, in_bp, sol11dsngl_restart,
		(caddr_t)sol11dsnglp) != DDI_SUCCESS) {

		sol11dsnglp->ds_bp = in_bp;
		mutex_exit(&sol11dsnglp->ds_mutex);
		return (0);
	}
	sol11dsnglp->ds_outcnt++;
	if (sol11dsnglp->ds_kstat)
		kstat_waitq_to_runq(KSTAT_IO_PTR(sol11dsnglp->ds_kstat));
	mutex_exit(&sol11dsnglp->ds_mutex);
	TGCOM_TRANSPORT(tgcom_objp, in_bp);
	return (0);
}

static int
sol11dsngl_deque(opaque_t queuep, struct buf *in_bp)
{
	struct sol11fc_data *sol11dsnglp = (struct sol11fc_data *)queuep;
	opaque_t tgcom_objp;
	opaque_t sol11que_objp;
	struct	 buf *bp;

	sol11que_objp   = sol11dsnglp->ds_queobjp;
	tgcom_objp = sol11dsnglp->ds_tgcomobjp;

	mutex_enter(&sol11dsnglp->ds_mutex);
	if (in_bp) {
		sol11dsnglp->ds_outcnt--;
		if (sol11dsnglp->ds_kstat) {
			if (in_bp->b_flags & B_READ) {
				KSTAT_IO_PTR(sol11dsnglp->ds_kstat)->reads++;
				KSTAT_IO_PTR(sol11dsnglp->ds_kstat)->nread +=
				    (in_bp->b_bcount - in_bp->b_resid);
			} else {
				KSTAT_IO_PTR(sol11dsnglp->ds_kstat)->writes++;
				KSTAT_IO_PTR(sol11dsnglp->ds_kstat)->nwritten +=
				    (in_bp->b_bcount - in_bp->b_resid);
			}
			kstat_runq_exit(KSTAT_IO_PTR(sol11dsnglp->ds_kstat));
		}
	}
	for (;;) {
		if (!sol11dsnglp->ds_bp)
			sol11dsnglp->ds_bp = SOL11QUE_DEL(sol11que_objp);
		if (!sol11dsnglp->ds_bp ||
		    (TGCOM_PKT(tgcom_objp, sol11dsnglp->ds_bp, sol11dsngl_restart,
		    (caddr_t)sol11dsnglp) != DDI_SUCCESS) ||
		    sol11dsnglp->ds_outcnt) {
			mutex_exit(&sol11dsnglp->ds_mutex);
			return (0);
		}
		sol11dsnglp->ds_outcnt++;
		bp = sol11dsnglp->ds_bp;
		sol11dsnglp->ds_bp = SOL11QUE_DEL(sol11que_objp);
		if (sol11dsnglp->ds_kstat)
			kstat_waitq_to_runq(KSTAT_IO_PTR(sol11dsnglp->ds_kstat));
		mutex_exit(&sol11dsnglp->ds_mutex);

		TGCOM_TRANSPORT(tgcom_objp, bp);

		if (!mutex_tryenter(&sol11dsnglp->ds_mutex))
			return (0);
	}
}

static int
sol11dsngl_restart(struct sol11fc_data *sol11dsnglp)
{
	(void) sol11dsngl_deque(sol11dsnglp, NULL);
	return (-1);
}


/*
 *	Multiple Commands per Device
 */
/*
 * Local Function Prototypes
 */
static int sol11dmult_restart();

static int sol11dmult_enque(opaque_t, struct buf *);
static int sol11dmult_deque(opaque_t, struct buf *);

struct 	sol11flc_objops sol11dmult_ops = {
	sol11fc_init,
	sol11fc_free,
	sol11dmult_enque,
	sol11dmult_deque,
	sol11fc_start_kstat,
	sol11fc_stop_kstat,
	0, 0
};

struct sol11flc_obj *
sol11dmult_create()
{
	return (sol11fc_create((struct sol11flc_objops *)&sol11dmult_ops));

}


/*
 * Some of the object management functions SOL11QUE_ADD() and SOL11QUE_DEL()
 * do not accquire lock.
 * They depend on sol11dmult_enque(), sol11dmult_deque() to do all locking.
 * If this changes we have to grab locks in sol11qmerge_add() and sol11qmerge_del().
 */
static int
sol11dmult_enque(opaque_t queuep, struct buf *in_bp)
{
	struct sol11fc_data *dmultp = (struct sol11fc_data *)queuep;
	opaque_t tgcom_objp;
	opaque_t sol11que_objp;

	sol11que_objp   = dmultp->ds_queobjp;
	tgcom_objp = dmultp->ds_tgcomobjp;

	if (!in_bp)
		return (0);
	mutex_enter(&dmultp->ds_mutex);
	if ((dmultp->ds_outcnt >= dmultp->ds_waitcnt) || dmultp->ds_bp) {
		SOL11QUE_ADD(sol11que_objp, in_bp);
		if (dmultp->ds_kstat) {
			kstat_waitq_enter(KSTAT_IO_PTR(dmultp->ds_kstat));
		}
		mutex_exit(&dmultp->ds_mutex);
		return (0);
	}
	if (dmultp->ds_kstat) {
		kstat_waitq_enter(KSTAT_IO_PTR(dmultp->ds_kstat));
	}

	if (TGCOM_PKT(tgcom_objp, in_bp, sol11dmult_restart,
		(caddr_t)dmultp) != DDI_SUCCESS) {

		dmultp->ds_bp = in_bp;
		mutex_exit(&dmultp->ds_mutex);
		return (0);
	}
	dmultp->ds_outcnt++;
	if (dmultp->ds_kstat)
		kstat_waitq_to_runq(KSTAT_IO_PTR(dmultp->ds_kstat));
	mutex_exit(&dmultp->ds_mutex);

	TGCOM_TRANSPORT(tgcom_objp, in_bp);
	return (0);
}

static int
sol11dmult_deque(opaque_t queuep, struct buf *in_bp)
{
	struct sol11fc_data *dmultp = (struct sol11fc_data *)queuep;
	opaque_t tgcom_objp;
	opaque_t sol11que_objp;
	struct	 buf *bp;

	sol11que_objp = dmultp->ds_queobjp;
	tgcom_objp = dmultp->ds_tgcomobjp;

	mutex_enter(&dmultp->ds_mutex);
	if (in_bp) {
		dmultp->ds_outcnt--;
		if (dmultp->ds_kstat) {
			if (in_bp->b_flags & B_READ) {
				KSTAT_IO_PTR(dmultp->ds_kstat)->reads++;
				KSTAT_IO_PTR(dmultp->ds_kstat)->nread +=
				    (in_bp->b_bcount - in_bp->b_resid);
			} else {
				KSTAT_IO_PTR(dmultp->ds_kstat)->writes++;
				KSTAT_IO_PTR(dmultp->ds_kstat)->nwritten +=
				    (in_bp->b_bcount - in_bp->b_resid);
			}
			kstat_runq_exit(KSTAT_IO_PTR(dmultp->ds_kstat));
		}
	}

	for (;;) {

#ifdef	FLC_DEBUG
		if ((curthread->t_intr) && (!dmultp->ds_bp) &&
		    (!dmultp->ds_outcnt))
			sol11flc_malloc_intr++;
#endif

		if (!dmultp->ds_bp)
			dmultp->ds_bp = SOL11QUE_DEL(sol11que_objp);
		if (!dmultp->ds_bp ||
		    (TGCOM_PKT(tgcom_objp, dmultp->ds_bp, sol11dmult_restart,
		    (caddr_t)dmultp) != DDI_SUCCESS) ||
		    (dmultp->ds_outcnt >= dmultp->ds_waitcnt)) {
			mutex_exit(&dmultp->ds_mutex);
			return (0);
		}
		dmultp->ds_outcnt++;
		bp = dmultp->ds_bp;
		dmultp->ds_bp = SOL11QUE_DEL(sol11que_objp);

		if (dmultp->ds_kstat)
			kstat_waitq_to_runq(KSTAT_IO_PTR(dmultp->ds_kstat));

		mutex_exit(&dmultp->ds_mutex);

		TGCOM_TRANSPORT(tgcom_objp, bp);

		if (!mutex_tryenter(&dmultp->ds_mutex))
			return (0);
	}
}

static int
sol11dmult_restart(struct sol11fc_data *dmultp)
{
	(void) sol11dmult_deque(dmultp, NULL);
	return (-1);
}

/*
 *	Duplexed Commands per Device: Read Queue and Write Queue
 */
/*
 * Local Function Prototypes
 */
static int sol11duplx_restart();

static int sol11duplx_init(opaque_t queuep, opaque_t tgcom_objp, opaque_t sol11que_objp,
    void *lkarg);
static int sol11duplx_free(struct sol11flc_obj *flcobjp);
static int sol11duplx_enque(opaque_t queuep, struct buf *bp);
static int sol11duplx_deque(opaque_t queuep, struct buf *bp);

struct 	sol11flc_objops sol11duplx_ops = {
	sol11duplx_init,
	sol11duplx_free,
	sol11duplx_enque,
	sol11duplx_deque,
	sol11fc_start_kstat,
	sol11fc_stop_kstat,
	0, 0
};

struct sol11flc_obj *
sol11duplx_create()
{
	struct	sol11flc_obj *flcobjp;
	struct	sol11duplx_data *fcdp;

	flcobjp = kmem_zalloc((sizeof (*flcobjp) + sizeof (*fcdp)), KM_NOSLEEP);
	if (!flcobjp)
		return (NULL);

	fcdp = (struct sol11duplx_data *)(flcobjp+1);
	flcobjp->sol11flc_data = (opaque_t)fcdp;
	flcobjp->sol11flc_ops  = &sol11duplx_ops;

	fcdp->ds_writeq.sol11fc_qobjp = sol11qfifo_create();
	if (!(fcdp->ds_writeq.sol11fc_qobjp = sol11qfifo_create())) {
		kmem_free(flcobjp, (sizeof (*flcobjp) + sizeof (*fcdp)));
		return (NULL);
	}
	return (flcobjp);
}

static int
sol11duplx_free(struct sol11flc_obj *flcobjp)
{
	struct sol11duplx_data *fcdp;

	fcdp = (struct sol11duplx_data *)flcobjp->sol11flc_data;
	if (fcdp->ds_writeq.sol11fc_qobjp) {
		SOL11QUE_FREE(fcdp->ds_writeq.sol11fc_qobjp);
	}
	if (fcdp->ds_readq.sol11fc_qobjp)
		SOL11QUE_FREE(fcdp->ds_readq.sol11fc_qobjp);
	if (fcdp->ds_tgcomobjp) {
		TGCOM_FREE(fcdp->ds_tgcomobjp);
		mutex_destroy(&fcdp->ds_mutex);
	}
	kmem_free(flcobjp, (sizeof (*flcobjp) + sizeof (*fcdp)));
	return (0);
}

static int
sol11duplx_init(opaque_t queuep, opaque_t tgcom_objp, opaque_t sol11que_objp, void *lkarg)
{
	struct sol11duplx_data *fcdp = (struct sol11duplx_data *)queuep;
	fcdp->ds_tgcomobjp = tgcom_objp;
	fcdp->ds_readq.sol11fc_qobjp = sol11que_objp;

	SOL11QUE_INIT(sol11que_objp, lkarg);
	SOL11QUE_INIT(fcdp->ds_writeq.sol11fc_qobjp, lkarg);
	TGCOM_INIT(tgcom_objp);

	mutex_init(&fcdp->ds_mutex, NULL, MUTEX_DRIVER, lkarg);

	fcdp->ds_writeq.sol11fc_maxcnt = DUPLX_MAXCNT;
	fcdp->ds_readq.sol11fc_maxcnt  = DUPLX_MAXCNT;

	/* queues point to each other for round robin */
	fcdp->ds_readq.next = &fcdp->ds_writeq;
	fcdp->ds_writeq.next = &fcdp->ds_readq;

	return (DDI_SUCCESS);
}

static int
sol11duplx_enque(opaque_t queuep, struct buf *in_bp)
{
	struct sol11duplx_data *duplxp = (struct sol11duplx_data *)queuep;
	opaque_t tgcom_objp;
	struct sol11fc_que *activeq;
	struct buf *bp;

	mutex_enter(&duplxp->ds_mutex);
	if (in_bp) {
		if (duplxp->ds_kstat) {
			kstat_waitq_enter(KSTAT_IO_PTR(duplxp->ds_kstat));
		}
		if (in_bp->b_flags & B_READ)
			activeq = &duplxp->ds_readq;
		else
			activeq = &duplxp->ds_writeq;

		SOL11QUE_ADD(activeq->sol11fc_qobjp, in_bp);
	} else {
		activeq = &duplxp->ds_readq;
	}

	tgcom_objp = duplxp->ds_tgcomobjp;

	for (;;) {
		if (!activeq->sol11fc_bp)
			activeq->sol11fc_bp = SOL11QUE_DEL(activeq->sol11fc_qobjp);
		if (!activeq->sol11fc_bp ||
		    (TGCOM_PKT(tgcom_objp, activeq->sol11fc_bp, sol11duplx_restart,
		    (caddr_t)duplxp) != DDI_SUCCESS) ||
		    (activeq->sol11fc_outcnt >= activeq->sol11fc_maxcnt)) {

			/* switch read/write queues */
			activeq = activeq->next;
			if (!activeq->sol11fc_bp)
				activeq->sol11fc_bp = SOL11QUE_DEL(activeq->sol11fc_qobjp);
			if (!activeq->sol11fc_bp ||
			    (TGCOM_PKT(tgcom_objp, activeq->sol11fc_bp,
			    sol11duplx_restart, (caddr_t)duplxp) != DDI_SUCCESS) ||
			    (activeq->sol11fc_outcnt >= activeq->sol11fc_maxcnt)) {
				mutex_exit(&duplxp->ds_mutex);
				return (0);
			}
		}

		activeq->sol11fc_outcnt++;
		bp = activeq->sol11fc_bp;
		activeq->sol11fc_bp = NULL;

		if (duplxp->ds_kstat)
			kstat_waitq_to_runq(KSTAT_IO_PTR(duplxp->ds_kstat));
		mutex_exit(&duplxp->ds_mutex);

		TGCOM_TRANSPORT(tgcom_objp, bp);

		if (!mutex_tryenter(&duplxp->ds_mutex))
			return (0);

		activeq = activeq->next;
	}
}

static int
sol11duplx_deque(opaque_t queuep, struct buf *in_bp)
{
	struct sol11duplx_data *duplxp = (struct sol11duplx_data *)queuep;
	opaque_t tgcom_objp;
	struct sol11fc_que *activeq;
	struct buf *bp;

	mutex_enter(&duplxp->ds_mutex);

	tgcom_objp = duplxp->ds_tgcomobjp;

	if (in_bp->b_flags & B_READ)
		activeq = &duplxp->ds_readq;
	else
		activeq = &duplxp->ds_writeq;
	activeq->sol11fc_outcnt--;

	if (duplxp->ds_kstat) {
		if (in_bp->b_flags & B_READ) {
			KSTAT_IO_PTR(duplxp->ds_kstat)->reads++;
			KSTAT_IO_PTR(duplxp->ds_kstat)->nread +=
			    (in_bp->b_bcount - in_bp->b_resid);
		} else {
			KSTAT_IO_PTR(duplxp->ds_kstat)->writes++;
			KSTAT_IO_PTR(duplxp->ds_kstat)->nwritten +=
			    (in_bp->b_bcount - in_bp->b_resid);
		}
		kstat_runq_exit(KSTAT_IO_PTR(duplxp->ds_kstat));
	}

	for (;;) {

		/* if needed, try to pull request off a queue */
		if (!activeq->sol11fc_bp)
			activeq->sol11fc_bp = SOL11QUE_DEL(activeq->sol11fc_qobjp);

		if (!activeq->sol11fc_bp ||
		    (TGCOM_PKT(tgcom_objp, activeq->sol11fc_bp, sol11duplx_restart,
		    (caddr_t)duplxp) != DDI_SUCCESS) ||
		    (activeq->sol11fc_outcnt >= activeq->sol11fc_maxcnt)) {

			activeq = activeq->next;
			if (!activeq->sol11fc_bp)
				activeq->sol11fc_bp = SOL11QUE_DEL(activeq->sol11fc_qobjp);

			if (!activeq->sol11fc_bp ||
			    (TGCOM_PKT(tgcom_objp, activeq->sol11fc_bp,
			    sol11duplx_restart, (caddr_t)duplxp) != DDI_SUCCESS) ||
			    (activeq->sol11fc_outcnt >= activeq->sol11fc_maxcnt)) {
				mutex_exit(&duplxp->ds_mutex);
				return (0);
			}
		}

		activeq->sol11fc_outcnt++;
		bp = activeq->sol11fc_bp;
		activeq->sol11fc_bp = NULL;

		if (duplxp->ds_kstat)
			kstat_waitq_to_runq(KSTAT_IO_PTR(duplxp->ds_kstat));

		mutex_exit(&duplxp->ds_mutex);

		TGCOM_TRANSPORT(tgcom_objp, bp);

		if (!mutex_tryenter(&duplxp->ds_mutex))
			return (0);

		activeq = activeq->next;
	}
}

static int
sol11duplx_restart(struct sol11duplx_data *duplxp)
{
	(void) sol11duplx_enque(duplxp, NULL);
	return (-1);
}

/*
 *	Tagged queueing flow control
 */
/*
 * Local Function Prototypes
 */

struct 	sol11flc_objops sol11adapt_ops = {
	sol11fc_init,
	sol11fc_free,
	sol11dmult_enque,
	sol11dmult_deque,
	sol11fc_start_kstat,
	sol11fc_stop_kstat,
	0, 0
};

struct sol11flc_obj *
sol11adapt_create()
{
	return (sol11fc_create((struct sol11flc_objops *)&sol11adapt_ops));

}

/*
 *	Common Queue functions
 */

/*
 * 	Local static data
 */
#ifdef	Q_DEBUG
#define	DENT	0x0001
#define	DERR	0x0002
#define	DIO	0x0004
static	int	sol11que_debug = DENT|DERR|DIO;

#endif	/* Q_DEBUG */
/*
 * 	Local Function Prototypes
 */
static struct sol11que_obj *sol11que_create(struct sol11que_objops *qopsp);
static int sol11que_init(struct sol11que_data *qfp, void *lkarg);
static int sol11que_free(struct sol11que_obj *queobjp);
static struct buf *sol11que_del(struct sol11que_data *qfp);

static struct sol11que_obj *
sol11que_create(struct sol11que_objops *qopsp)
{
	struct	sol11que_data *qfp;
	struct	sol11que_obj *queobjp;

	queobjp = kmem_zalloc((sizeof (*queobjp) + sizeof (*qfp)), KM_NOSLEEP);
	if (!queobjp)
		return (NULL);

	queobjp->que_ops = qopsp;
	qfp = (struct sol11que_data *)(queobjp+1);
	queobjp->sol11que_data = (opaque_t)qfp;

	return ((opaque_t)queobjp);
}

static int
sol11que_init(struct sol11que_data *qfp, void *lkarg)
{
	mutex_init(&qfp->q_mutex, NULL, MUTEX_DRIVER, lkarg);
	return (DDI_SUCCESS);
}

static int
sol11que_free(struct sol11que_obj *queobjp)
{
	struct	sol11que_data *qfp;

	qfp = (struct sol11que_data *)queobjp->sol11que_data;
	mutex_destroy(&qfp->q_mutex);
	kmem_free(queobjp, (sizeof (*queobjp) + sizeof (struct sol11que_data)));
	return (0);
}

static struct buf *
sol11que_del(struct sol11que_data *qfp)
{
	struct buf *bp;

	bp = qfp->q_tab.b_actf;
	if (bp) {
		qfp->q_tab.b_actf = bp->av_forw;
		if (!qfp->q_tab.b_actf)
			qfp->q_tab.b_actl = NULL;
		bp->av_forw = 0;
	}
	return (bp);
}



/*
 *	Qmerge
 * 	Local Function Prototypes
 */
static int sol11qmerge_add(), sol11qmerge_free();
static struct buf *sol11qmerge_del(struct sol11que_data *qfp);

struct 	sol11que_objops sol11qmerge_ops = {
	sol11que_init,
	sol11qmerge_free,
	sol11qmerge_add,
	sol11qmerge_del,
	0, 0
};

/* fields in diskhd */
#define	hd_cnt			b_back
#define	hd_private		b_forw
#define	hd_flags		b_flags
#define	hd_sync_next		av_forw
#define	hd_async_next		av_back

#define	hd_sync2async		sync_async_ratio

#define	QNEAR_FORWARD		0x01
#define	QNEAR_BACKWARD		0x02
#define	QNEAR_ASYNCONLY		0x04
#define	QNEAR_ASYNCALSO		0x08

#define	DBLK(bp) ((unsigned long)(bp)->b_private)

#define	BP_LT_BP(a, b) (DBLK(a) < DBLK(b))
#define	BP_GT_BP(a, b) (DBLK(a) > DBLK(b))
#define	BP_LT_HD(a, b) (DBLK(a) < (unsigned long)((b)->hd_private))
#define	BP_GT_HD(a, b) (DBLK(a) > (unsigned long)((b)->hd_private))
#define	QNEAR_ASYNC	(QNEAR_ASYNCONLY|QNEAR_ASYNCALSO)

#define	SYNC2ASYNC(a) ((a)->q_tab.hd_cnt)


/*
 * sol11qmerge implements a two priority queue, the low priority queue holding ASYNC
 * write requests, while the rest are queued in the high priority sync queue.
 * Requests on the async queue would be merged if possible.
 * By default sol11qmerge2wayscan is 1, indicating an elevator algorithm. When
 * this variable is set to zero, it has the following side effects.
 * 1. We assume fairness is the number one issue.
 * 2. The next request to be picked indicates current head position.
 *
 * sol11qmerge_sync2async indicates the ratio of scans of high prioriy
 * sync queue to low priority async queue.
 *
 * When sol11qmerge variables have the following values it defaults to sol11qsort
 *
 * sol11qmerge1pri = 1, sol11qmerge2wayscan = 0, sol11qmerge_max_merge = 0
 *
 */
static int	sol11qmerge_max_merge = 128 * 1024;
static intptr_t	sol11qmerge_sync2async = 4;
static int	sol11qmerge2wayscan = 1;
static int	sol11qmerge1pri = 0;
static int	sol11qmerge_merge = 0;

/*
 * 	Local static data
 */
struct sol11que_obj *
sol11qmerge_create()
{
	struct sol11que_data *qfp;
	struct sol11que_obj *queobjp;

	queobjp = kmem_zalloc((sizeof (*queobjp) + sizeof (*qfp)), KM_NOSLEEP);
	if (!queobjp)
		return (NULL);

	queobjp->que_ops = &sol11qmerge_ops;
	qfp = (struct sol11que_data *)(queobjp+1);
	qfp->q_tab.hd_private = qfp->q_tab.hd_private = 0;
	qfp->q_tab.hd_sync_next = qfp->q_tab.hd_async_next = NULL;
	qfp->q_tab.hd_cnt = (void *)sol11qmerge_sync2async;
	queobjp->sol11que_data = (opaque_t)qfp;

	return ((opaque_t)queobjp);
}

static int
sol11qmerge_free(struct sol11que_obj *queobjp)
{
	struct	sol11que_data *qfp;

	qfp = (struct sol11que_data *)queobjp->sol11que_data;
	mutex_destroy(&qfp->q_mutex);
	kmem_free(queobjp, (sizeof (*queobjp) + sizeof (*qfp)));
	return (0);
}

static int
sol11qmerge_can_merge(bp1, bp2)
struct	buf *bp1, *bp2;
{
	const int paw_flags = B_PAGEIO | B_ASYNC | B_WRITE;

	if ((bp1->b_un.b_addr != 0) || (bp2->b_un.b_addr != 0) ||
	    ((bp1->b_flags & (paw_flags | B_REMAPPED)) != paw_flags) ||
	    ((bp2->b_flags & (paw_flags | B_REMAPPED)) != paw_flags) ||
	    (bp1->b_bcount & PAGEOFFSET) || (bp2->b_bcount & PAGEOFFSET) ||
	    (bp1->b_bcount + bp2->b_bcount > sol11qmerge_max_merge))
		return (0);

	if ((DBLK(bp2) + bp2->b_bcount / DEV_BSIZE == DBLK(bp1)) ||
	    (DBLK(bp1) + bp1->b_bcount / DEV_BSIZE == DBLK(bp2)))
		return (1);
	else
		return (0);
}

static void
sol11qmerge_mergesetup(bp_merge, bp)
struct	buf *bp_merge, *bp;
{
	struct	buf *bp1;
	struct	page *pp, *pp_merge, *pp_merge_prev;
	int	forward;

	sol11qmerge_merge++;
	forward = DBLK(bp_merge) < DBLK(bp);

	bp_merge->b_bcount += bp->b_bcount;

	pp = bp->b_pages;
	pp_merge = bp_merge->b_pages;

	pp_merge_prev = pp_merge->p_prev;

	pp_merge->p_prev->p_next = pp;
	pp_merge->p_prev = pp->p_prev;
	pp->p_prev->p_next = pp_merge;
	pp->p_prev = pp_merge_prev;

	bp1 = bp_merge->b_forw;

	bp1->av_back->av_forw = bp;
	bp->av_back = bp1->av_back;
	bp1->av_back = bp;
	bp->av_forw = bp1;

	if (!forward) {
		bp_merge->b_forw = bp;
		bp_merge->b_pages = pp;
		bp_merge->b_private = bp->b_private;
	}
}

static void
sol11que_insert(struct sol11que_data *qfp, struct buf *bp)
{
	struct buf	*bp1, *bp_start, *lowest_bp, *highest_bp;
	uintptr_t	highest_blk, lowest_blk;
	struct buf	**async_bpp, **sync_bpp, **bpp;
	struct diskhd	*dp = &qfp->q_tab;

	sync_bpp = &dp->hd_sync_next;
	async_bpp = &dp->hd_async_next;
	/*
	 * The ioctl used by the format utility requires that bp->av_back be
	 * preserved.
	 */
	if (bp->av_back)
		bp->b_error = (intptr_t)bp->av_back;
	if (!sol11qmerge1pri &&
	    ((bp->b_flags & (B_ASYNC|B_READ|B_FREE)) == B_ASYNC)) {
		bpp = &dp->hd_async_next;
	} else {
		bpp = &dp->hd_sync_next;
	}


	if ((bp1 = *bpp) == NULL) {
		*bpp = bp;
		bp->av_forw = bp->av_back = bp;
		if ((bpp == async_bpp) && (*sync_bpp == NULL)) {
			dp->hd_flags |= QNEAR_ASYNCONLY;
		} else if (bpp == sync_bpp) {
			dp->hd_flags &= ~QNEAR_ASYNCONLY;
			if (*async_bpp) {
				dp->hd_flags |= QNEAR_ASYNCALSO;
			}
		}
		return;
	}
	bp_start = bp1;
	if (DBLK(bp) < DBLK(bp1)) {
		lowest_blk = DBLK(bp1);
		lowest_bp = bp1;
		do {
			if (DBLK(bp) > DBLK(bp1)) {
				bp->av_forw = bp1->av_forw;
				bp1->av_forw->av_back = bp;
				bp1->av_forw = bp;
				bp->av_back = bp1;

				if (((bpp == async_bpp) &&
				    (dp->hd_flags & QNEAR_ASYNC)) ||
				    (bpp == sync_bpp)) {
					if (!(dp->hd_flags & QNEAR_BACKWARD) &&
					    BP_GT_HD(bp, dp)) {
						*bpp = bp;
					}
				}
				return;
			} else if (DBLK(bp1) < lowest_blk) {
				lowest_bp = bp1;
				lowest_blk = DBLK(bp1);
			}
		} while ((DBLK(bp1->av_back) < DBLK(bp1)) &&
		    ((bp1 = bp1->av_back) != bp_start));
		bp->av_forw = lowest_bp;
		lowest_bp->av_back->av_forw = bp;
		bp->av_back = lowest_bp->av_back;
		lowest_bp->av_back = bp;
		if ((bpp == async_bpp) && !(dp->hd_flags & QNEAR_ASYNC)) {
			*bpp = bp;
		} else if (!(dp->hd_flags & QNEAR_BACKWARD) &&
		    BP_GT_HD(bp, dp)) {
			*bpp = bp;
		}
	} else {
		highest_blk = DBLK(bp1);
		highest_bp = bp1;
		do {
			if (DBLK(bp) < DBLK(bp1)) {
				bp->av_forw = bp1;
				bp1->av_back->av_forw = bp;
				bp->av_back = bp1->av_back;
				bp1->av_back = bp;
				if (((bpp == async_bpp) &&
				    (dp->hd_flags & QNEAR_ASYNC)) ||
				    (bpp == sync_bpp)) {
					if ((dp->hd_flags & QNEAR_BACKWARD) &&
					    BP_LT_HD(bp, dp)) {
						*bpp = bp;
					}
				}
				return;
			} else if (DBLK(bp1) > highest_blk) {
				highest_bp = bp1;
				highest_blk = DBLK(bp1);
			}
		} while ((DBLK(bp1->av_forw) > DBLK(bp1)) &&
		    ((bp1 = bp1->av_forw) != bp_start));
		bp->av_back = highest_bp;
		highest_bp->av_forw->av_back = bp;
		bp->av_forw = highest_bp->av_forw;
		highest_bp->av_forw = bp;

		if (((bpp == sync_bpp) ||
		    ((bpp == async_bpp) && (dp->hd_flags & QNEAR_ASYNC))) &&
		    (dp->hd_flags & QNEAR_BACKWARD) && (BP_LT_HD(bp, dp)))
			*bpp = bp;
	}
}

/*
 * sol11dmult_enque() holds dmultp->ds_mutex lock, so we dont grab
 * lock here. If sol11dmult_enque() changes we will have to visit
 * this function again
 */
static int
sol11qmerge_add(struct sol11que_data *qfp, struct buf *bp)
{

	sol11que_insert(qfp, bp);
	return (++qfp->q_cnt);
}

static int
sol11qmerge_iodone(struct buf *bp)
{
	struct buf *bp1;
	struct	page *pp, *pp1, *tmp_pp;

	if (bp->b_flags & B_REMAPPED)
		bp_mapout(bp);

	bp1 = bp->b_forw;
	do {
		bp->b_forw = bp1->av_forw;
		bp1->av_forw->av_back = bp1->av_back;
		bp1->av_back->av_forw = bp1->av_forw;
		pp = (page_t *)bp1->b_pages;
		pp1 = bp->b_forw->b_pages;

		tmp_pp = pp->p_prev;
		pp->p_prev = pp1->p_prev;
		pp->p_prev->p_next = pp;

		pp1->p_prev = tmp_pp;
		pp1->p_prev->p_next = pp1;

		if (bp->b_flags & B_ERROR) {
			bp1->b_error = bp->b_error;
			bp1->b_flags |= B_ERROR;
		}

		biodone(bp1);
	} while ((bp1 = bp->b_forw) != bp->b_forw->av_forw);

	biodone(bp1);
	kmem_free(bp, sizeof (*bp));
	return (0);
}




static struct buf *
sol11qmerge_nextbp(struct sol11que_data *qfp, struct buf *bp_merge, int *can_merge)
{
	intptr_t	private, cnt;
	int		flags;
	struct		buf *sync_bp, *async_bp, *bp;
	struct		buf **sync_bpp, **async_bpp, **bpp;
	struct		diskhd *dp = &qfp->q_tab;

	if (qfp->q_cnt == 0) {
		return (NULL);
	}
	flags = qfp->q_tab.hd_flags;
	sync_bpp = &qfp->q_tab.hd_sync_next;
	async_bpp = &qfp->q_tab.hd_async_next;

begin_nextbp:
	if (flags & QNEAR_ASYNCONLY) {
		bp = *async_bpp;
		private = DBLK(bp);
		if (bp_merge && !sol11qmerge_can_merge(bp, bp_merge)) {
			return (NULL);
		} else if (bp->av_forw == bp) {
			bp->av_forw = bp->av_back = NULL;
			flags &= ~(QNEAR_ASYNCONLY | QNEAR_BACKWARD);
			private = 0;
		} else if (flags & QNEAR_BACKWARD) {
			if (DBLK(bp) < DBLK(bp->av_back)) {
				flags &= ~QNEAR_BACKWARD;
				private = 0;
			}
		} else if (DBLK(bp) > DBLK(bp->av_forw)) {
			if (sol11qmerge2wayscan) {
				flags |= QNEAR_BACKWARD;
			} else {
				private = 0;
			}
		} else if (sol11qmerge2wayscan == 0) {
			private = DBLK(bp->av_forw);
		}
		bpp = async_bpp;

	} else if (flags & QNEAR_ASYNCALSO) {
		sync_bp = *sync_bpp;
		async_bp = *async_bpp;
		if (flags & QNEAR_BACKWARD) {
			if (BP_GT_HD(sync_bp, dp) && BP_GT_HD(async_bp, dp)) {
				flags &= ~(QNEAR_BACKWARD|QNEAR_ASYNCALSO);
				*sync_bpp = sync_bp->av_forw;
				*async_bpp = async_bp->av_forw;
				SYNC2ASYNC(qfp) = (void *)sol11qmerge_sync2async;
				qfp->q_tab.hd_private = 0;
				goto begin_nextbp;
			}
			if (BP_LT_HD(async_bp, dp) && BP_LT_HD(sync_bp, dp)) {
				if (BP_GT_BP(async_bp, sync_bp)) {
					bpp = async_bpp;
					bp = *async_bpp;
				} else {
					bpp = sync_bpp;
					bp = *sync_bpp;
				}
			} else if (BP_LT_HD(async_bp, dp)) {
				bpp = async_bpp;
				bp = *async_bpp;
			} else {
				bpp = sync_bpp;
				bp = *sync_bpp;
			}
		} else {
			if (BP_LT_HD(sync_bp, dp) && BP_LT_HD(async_bp, dp)) {
				if (sol11qmerge2wayscan) {
					flags |= QNEAR_BACKWARD;
					*sync_bpp = sync_bp->av_back;
					*async_bpp = async_bp->av_back;
					goto begin_nextbp;
				} else {
					flags &= ~QNEAR_ASYNCALSO;
					SYNC2ASYNC(qfp) =
						(void *)sol11qmerge_sync2async;
					qfp->q_tab.hd_private = 0;
					goto begin_nextbp;
				}
			}
			if (BP_GT_HD(async_bp, dp) && BP_GT_HD(sync_bp, dp)) {
				if (BP_LT_BP(async_bp, sync_bp)) {
					bpp = async_bpp;
					bp = *async_bpp;
				} else {
					bpp = sync_bpp;
					bp = *sync_bpp;
				}
			} else if (BP_GT_HD(async_bp, dp)) {
				bpp = async_bpp;
				bp = *async_bpp;
			} else {
				bpp = sync_bpp;
				bp = *sync_bpp;
			}
		}
		if (bp_merge && !sol11qmerge_can_merge(bp, bp_merge)) {
			return (NULL);
		} else if (bp->av_forw == bp) {
			bp->av_forw = bp->av_back = NULL;
			flags &= ~QNEAR_ASYNCALSO;
			if (bpp == async_bpp) {
				SYNC2ASYNC(qfp) = (void *)sol11qmerge_sync2async;
			} else {
				flags |= QNEAR_ASYNCONLY;
			}
		}
		private = DBLK(bp);
	} else {
		bp = *sync_bpp;
		private = DBLK(bp);
		if (bp_merge && !sol11qmerge_can_merge(bp, bp_merge)) {
			return (NULL);
		} else if (bp->av_forw == bp) {
			private = 0;
			SYNC2ASYNC(qfp) = (void *)sol11qmerge_sync2async;
			bp->av_forw = bp->av_back = NULL;
			flags &= ~QNEAR_BACKWARD;
			if (*async_bpp)
				flags |= QNEAR_ASYNCONLY;
		} else if (flags & QNEAR_BACKWARD) {
			if (DBLK(bp) < DBLK(bp->av_back)) {
				flags &= ~QNEAR_BACKWARD;
				cnt = (intptr_t)SYNC2ASYNC(qfp);
				if (cnt > 0) {
					cnt--;
					SYNC2ASYNC(qfp) = (void *)cnt;
				} else {
					if (*async_bpp)
						flags |= QNEAR_ASYNCALSO;
					SYNC2ASYNC(qfp) =
						(void *)sol11qmerge_sync2async;
				}
				private = 0;
			}
		} else if (DBLK(bp) > DBLK(bp->av_forw)) {
			private = 0;
			if (sol11qmerge2wayscan) {
				flags |= QNEAR_BACKWARD;
				private = DBLK(bp);
			} else {
				cnt = (intptr_t)SYNC2ASYNC(qfp);
				if (cnt > 0) {
					cnt--;
					SYNC2ASYNC(qfp) = (void *)cnt;
				} else {
					if (*async_bpp)
						flags |= QNEAR_ASYNCALSO;
					SYNC2ASYNC(qfp) =
						(void *)sol11qmerge_sync2async;
				}
			}
		} else if (sol11qmerge2wayscan == 0) {
			private = DBLK(bp->av_forw);
		}
		bpp = sync_bpp;
	}

	if (bp->av_forw) {
		*can_merge = !(bp->b_flags & B_READ);
		if (flags & QNEAR_BACKWARD) {
			*bpp = bp->av_back;
			if ((DBLK(bp->av_back) +
			    bp->av_back->b_bcount / DEV_BSIZE) != DBLK(bp))
				*can_merge = 0;
		} else {
			*bpp = bp->av_forw;
			if ((DBLK(bp) + bp->b_bcount / DEV_BSIZE) !=
			    DBLK(bp->av_forw))
				*can_merge = 0;
		}
		bp->av_forw->av_back = bp->av_back;
		bp->av_back->av_forw = bp->av_forw;
		bp->av_forw = bp->av_back = NULL;
	} else {
		*bpp = NULL;
		*can_merge = 0;
	}
	qfp->q_tab.hd_private = (void *)private;
	qfp->q_cnt--;
	qfp->q_tab.hd_flags = flags;
	if (bp->b_error) {
		bp->av_back = (void *)(intptr_t)bp->b_error;
		bp->b_error = 0;
	}
	return (bp);
}

static struct buf *
sol11qmerge_del(struct sol11que_data *qfp)
{
	struct	buf *bp, *next_bp, *bp_merge;
	int	alloc_mergebp, merge;

	if (qfp->q_cnt == 0) {
		return (NULL);
	}

	bp_merge = bp = sol11qmerge_nextbp(qfp, NULL, &merge);
	alloc_mergebp = 1;
	while (merge && (next_bp = sol11qmerge_nextbp(qfp, bp_merge, &merge))) {
		if (alloc_mergebp) {
			bp_merge = kmem_alloc(sizeof (*bp_merge), KM_NOSLEEP);
			if (bp_merge == NULL) {
				mutex_exit(&qfp->q_mutex);
				return (bp);
			}
			bcopy(bp, bp_merge, sizeof (*bp_merge));
			bp_merge->b_iodone = sol11qmerge_iodone;
			bp_merge->b_forw = bp;
			bp_merge->b_back = (struct buf *)qfp;
			bp->av_forw = bp->av_back = bp;
			alloc_mergebp = 0;
		}
		sol11qmerge_mergesetup(bp_merge, next_bp);
	}
	return (bp_merge);
}


/*
 *	FIFO Queue functions
 */
/*
 * 	Local Function Prototypes
 */
static int sol11qfifo_add();

struct 	sol11que_objops sol11qfifo_ops = {
	sol11que_init,
	sol11que_free,
	sol11qfifo_add,
	sol11que_del,
	0, 0
};

/*
 * 	Local static data
 */
struct sol11que_obj *
sol11qfifo_create()
{
	return (sol11que_create((struct sol11que_objops *)&sol11qfifo_ops));
}

static int
sol11qfifo_add(struct sol11que_data *qfp, struct buf *bp)
{

	if (!qfp->q_tab.b_actf)
		qfp->q_tab.b_actf = bp;
	else
		qfp->q_tab.b_actl->av_forw = bp;
	qfp->q_tab.b_actl = bp;
	bp->av_forw = NULL;
	return (0);
}

/*
 *	One-Way-Scan Queue functions
 */
/*
 * 	Local Function Prototypes
 */
static int sol11qsort_add();
static struct buf *sol11qsort_del();
static void sol11oneway_scan_binary(struct diskhd *dp, struct buf *bp);

struct 	sol11que_objops sol11qsort_ops = {
	sol11que_init,
	sol11que_free,
	sol11qsort_add,
	sol11qsort_del,
	0, 0
};

/*
 * 	Local static data
 */
struct sol11que_obj *
sol11qsort_create()
{
	return (sol11que_create((struct sol11que_objops *)&sol11qsort_ops));
}

static int
sol11qsort_add(struct sol11que_data *qfp, struct buf *bp)
{
	qfp->q_cnt++;
	sol11oneway_scan_binary(&qfp->q_tab, bp);
	return (0);
}


#define	b_pasf	b_forw
#define	b_pasl	b_back
static void
sol11oneway_scan_binary(struct diskhd *dp, struct buf *bp)
{
	struct buf *ap;

	ap = dp->b_actf;
	if (ap == NULL) {
		dp->b_actf = bp;
		bp->av_forw = NULL;
		return;
	}
	if (DBLK(bp) < DBLK(ap)) {
		ap = dp->b_pasf;
		if ((ap == NULL) || (DBLK(bp) < DBLK(ap))) {
			dp->b_pasf = bp;
			bp->av_forw = ap;
			return;
		}
	}
	while (ap->av_forw) {
		if (DBLK(bp) < DBLK(ap->av_forw))
			break;
		ap = ap->av_forw;
	}
	bp->av_forw = ap->av_forw;
	ap->av_forw = bp;
}

static struct buf *
sol11qsort_del(struct sol11que_data *qfp)
{
	struct buf *bp;

	if (qfp->q_cnt == 0) {
		return (NULL);
	}
	qfp->q_cnt--;
	bp = qfp->q_tab.b_actf;
	qfp->q_tab.b_actf = bp->av_forw;
	bp->av_forw = 0;
	if (!qfp->q_tab.b_actf && qfp->q_tab.b_pasf) {
		qfp->q_tab.b_actf = qfp->q_tab.b_pasf;
		qfp->q_tab.b_pasf = NULL;
	}
	return (bp);
}

/*
 *	Tagged queueing
 */
/*
 * 	Local Function Prototypes
 */

struct 	sol11que_objops sol11qtag_ops = {
	sol11que_init,
	sol11que_free,
	sol11qsort_add,
	sol11qsort_del,
	0, 0
};

/*
 * 	Local static data
 */
struct sol11que_obj *
sol11qtag_create()
{
	return (sol11que_create((struct sol11que_objops *)&sol11qtag_ops));
}
