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
 * Copyright 2011 Grigale Ltd. All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Solaris native NBD administration utility
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "nbd.h"

#define	NBDCTL_DEV	"/dev/nbdctl"

int
main(int argc, char **argv)
{
	int	ctl;

	ctl = open(NBDCTL_DEV, O_RDWR);

	if (ctl == -1) {
		perror("nbdadm");
		exit(EXIT_FAILURE);
	}

	if (argc < 2) {
		printf("Attaching NBD device 1\n");
		if (ioctl(ctl, NBD_ATTACH_DEV) == -1) {
			perror("nbadm attach dev");
			exit(EXIT_FAILURE);
		}
	} else {
		printf("Detaching NBD device 1\n");
		if (ioctl(ctl, NBD_DETACH_DEV) == -1) {
			perror("nbadm detach dev");
			exit(EXIT_FAILURE);
		}
	}

	(void) close(ctl);
	exit(EXIT_SUCCESS);
}
