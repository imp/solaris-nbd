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

#ifndef _NBD_PROTOCOL_H
#define	_NBD_PROTOCOL_H

#include <sys/byteorder.h>
#include <sys/types.h>

/*
 * NBD protocol as stated in NBD project documentation
 * https://github.com/yoe/nbd/blob/master/doc/proto.txt
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	NBD_SIGNATURE	"NBDMAGIC"

#define NBD_NEW_STYLE	BE_64(0x49484156454F5054LL)


typedef struct {
	char		signature[8];	/* "NBDMAGIC" */
	uint64_t	magic;		/* new style */
	uint16_t	reserved;	/* zeroes */
} server_init_t;


typedef struct {
	uint32_t	reserved;	/* zeroes */
} client_init_t;


typedef {
	uint64_t	magic;
	uint32_t	option;
	uint32_t	optdatalen;
} option_header_t;


typedef {
	uint64_t	export_size;
	uint16_t	flags;
	uint8_t		reserved[124]'
} option_reply_t;


typedef {
	uint32_t	magic;
	uint32_t	type;
	uint64_t	handle;
	uint64_t	offset;
	uint32_t	len;
} nbd_request_t;


typedef {
	uint32_t	magic;
	uint32_t	error;
	uint64_t	handle;
} nbd_reply_t;


#define	NBD_OPT_EXPORT_NAME	0x00000001

#define	NBD_FLAG_HAS_FLAGS	0x0001
#define	NBD_FLAG_READ_ONLY	0x0002
#define	NBD_FLAG_SEND_FLUSH	0x0004
#define	NBD_FLAG_SEND_FUA	0x0008
#define	NBD_FLAG_ROTATIONAL	0x0010
#define	NBD_FLAG_SEND_TRIM	0x0020


#ifdef __cplusplus
}
#endif

#endif /* _NBD_PROTOCOL_H */
