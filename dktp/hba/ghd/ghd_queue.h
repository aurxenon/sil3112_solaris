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
 * Copyright 1999 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _GHD_QUEUE_H
#define	_GHD_QUEUE_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 *  A list of singly linked elements
 */

typedef struct L1el {
	struct L1el	*le_nextp;
	void		*le_datap;
} sol11L1el_t;

#define	L1EL_INIT(lep)	((lep)->le_nextp = NULL, (lep)->le_datap = 0)

typedef struct sol11L1_head {
	sol11L1el_t	*l1_headp;
	sol11L1el_t	*l1_tailp;
} sol11L1_t;

#define	L1HEADER_INIT(lp) (((lp)->l1_headp = NULL), ((lp)->l1_tailp = NULL))
#define	L1_EMPTY(lp)	((lp)->l1_headp == NULL)

void	 sol11L1_add(sol11L1_t *lp, sol11L1el_t *lep, void *datap);
void	 sol11L1_delete(sol11L1_t *lp, sol11L1el_t *lep);
void	*sol11L1_remove(sol11L1_t *lp);


/*
 * A list of doubly linked elements
 */

typedef struct sol11L2el {
	struct	sol11L2el	*l2_nextp;
	struct	sol11L2el	*l2_prevp;
	void		*l2_private;
} sol11L2el_t;

#define	SOL11L2_INIT(headp)	\
	(((headp)->l2_nextp = (headp)), ((headp)->l2_prevp = (headp)))

#define	SOL11L2_EMPTY(headp) ((headp)->l2_nextp == (headp))

void	sol11L2_add(sol11L2el_t *headp, sol11L2el_t *elementp, void *private);
void	sol11L2_delete(sol11L2el_t *elementp);
void	sol11L2_add_head(sol11L2el_t *headp, sol11L2el_t *elementp, void *private);
void	*sol11L2_remove_head(sol11L2el_t *headp);
void	*sol11L2_next(sol11L2el_t *elementp);


#ifdef	__cplusplus
}
#endif
#endif  /* _GHD_QUEUE_H */
