/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2011 Grigale Ltd.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _NBD_IMPL_H
#define	_NBD_IMPL_H

/*
 * Solaris NBD driver definitions.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef union nbd_sockaddr {
	struct sockaddr         sin;
	struct sockaddr_in      sin4;
	struct sockaddr_in6     sin6;
} nbd_sockaddr_t;

#define SIZEOF_SOCKADDR(so)	((so)->sa_family == AF_INET ? \
	sizeof (struct sockaddr_in) : sizeof (struct sockaddr_in6))


typedef struct {
	uint64_t	target;
	refstr_t	*name;
	int		id;
	ksocket_t	sock;
	nbd_sockaddr_t	addr;
} nbd_state_t;

typedef struct {
	dev_info_t	*dip;
	nbd_state_t	*nbds[NBD_MAX_DEVICES];
} nbd_ctl_state_t;

#define	NBD_INSTANCE2STATE(s, i)	(s.nbds[i])
#define NBD_INSTANCE_NAME(sp)		(char *)refstr_value(sp->name)

#ifdef __cplusplus
}
#endif

#endif /* _NBD_IMPL_H */
