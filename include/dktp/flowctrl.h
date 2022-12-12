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

#ifndef _SYS_DKTP_FLOWCTRL_H
#define	_SYS_DKTP_FLOWCTRL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	sol11flc_obj {
	opaque_t		sol11flc_data;
	struct sol11flc_objops	*sol11flc_ops;
};

struct	sol11flc_objops {
	int	(*sol11flc_init)(opaque_t, opaque_t, opaque_t, void *);
	int	(*sol11flc_free)(struct sol11flc_obj *);
	int	(*sol11flc_enque)(opaque_t, struct buf *);
	int	(*sol11flc_deque)(opaque_t, struct buf *);
	int	(*sol11flc_start_kstat)(opaque_t, char *, int);
	int	(*sol11flc_stop_kstat)(opaque_t);
	void	*sol11flc_resv[2];
};

struct sol11flc_obj *sol11dsngl_create();
struct sol11flc_obj *sol11dmult_create();
struct sol11flc_obj *sol11duplx_create();
struct sol11flc_obj *sol11adapt_create();

#define	FLC_INIT(X, tgcomobjp, queobjp, lkarg) \
	(*((struct sol11flc_obj *)(X))->sol11flc_ops->sol11flc_init) \
	(((struct sol11flc_obj *)(X))->sol11flc_data, (tgcomobjp), (queobjp), (lkarg))
#define	FLC_FREE(X) (*((struct sol11flc_obj *)(X))->sol11flc_ops->sol11flc_free) ((X))
#define	FLC_ENQUE(X, bp) (*((struct sol11flc_obj *)(X))->sol11flc_ops->sol11flc_enque) \
	(((struct sol11flc_obj *)(X))->sol11flc_data, (bp))
#define	FLC_DEQUE(X, bp) (*((struct sol11flc_obj *)(X))->sol11flc_ops->sol11flc_deque) \
	(((struct sol11flc_obj *)(X))->sol11flc_data, (bp))
#define	FLC_START_KSTAT(X, devtype, instance) \
	(*((struct sol11flc_obj *)(X))->sol11flc_ops->sol11flc_start_kstat)\
	(((struct sol11flc_obj *)(X))->sol11flc_data, (devtype), (instance))
#define	FLC_STOP_KSTAT(X) (*((struct sol11flc_obj *)(X))->sol11flc_ops->sol11flc_stop_kstat) \
	(((struct sol11flc_obj *)(X))->sol11flc_data)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_FLOWCTRL_H */
