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

#ifndef _NBD_H
#define	_NBD_H

/*
 * Solaris NBD driver interface definitions.
 */

#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	NBD_MAX_DEVICES	4096


/* /dev/nbdctl ioctl(2) codes */

#define	NBD_ATTACH_DEV	0x123456
#define	NBD_DETACH_DEV	0x123457

typedef struct {
	char			name[1024];
	struct sockaddr		*sin;
} nbd_cmd_t;

#ifdef __cplusplus
}
#endif

#endif /* _NBD_H */
