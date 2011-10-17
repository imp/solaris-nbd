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
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "nbd.h"

#define	NBDCTL_DEV	"/dev/nbdctl"

#define	NBD_PORT	"10809"

static int list;
static int attach;
static int detach;
static char *name;
static struct sockaddr *server;

static const struct option nbdadm_options[] = {
	{"list",	no_argument,		NULL,	'l'},
	{"attach",	no_argument,		NULL,	'a'},
	{"detach",	no_argument,		NULL,	'd'},
	{"name",	required_argument,	NULL,	'n'},
	{"server",	required_argument,	NULL,	's'},
	{NULL, 0, NULL, 0},
};

/*
int      getopt_clip(int, char * const *, const char *,
                    const struct option *, int *);
*/

static void
usage(void)
{
	printf("Usage: nbdadm attach|detach|list\n");
}


static struct sockaddr *
getserveraddr(char *server)
{
	struct sockaddr		*addr = NULL;
	struct addrinfo		hints;
	struct addrinfo		*res;
	int			rc;

	bzero(&hints, sizeof (hints));
	hints.ai_family		= AF_INET;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_flags		= AI_ADDRCONFIG | AI_NUMERICSERV;

	rc = getaddrinfo(server, NBD_PORT, &hints, &res);
	if (rc != 0) {
		printf("Failed to get server addrress - %s\n",
		    gai_strerror(rc));
		return (addr);
	}

	/* Loop through all the addresses to find the one that works */
	for (struct addrinfo *aip = res; aip != NULL; aip=aip->ai_next) {
		int sock;

		sock = socket(aip->ai_family, aip->ai_socktype,
		    aip->ai_protocol);
		if (sock == -1) {
			continue;
		}
	
		rc = connect(sock, aip->ai_addr, aip->ai_addrlen);
		(void) close(sock);
		if (rc == -1) {
			continue;
		}

		/* If we've reached this point we've got the working address */
		addr = malloc(aip->ai_addrlen);
		if (addr != NULL) {
			bcopy(aip->ai_addr, addr, aip->ai_addrlen);
		}
		break;
	}

	freeaddrinfo(res);
	return (addr);
}


int
main(int argc, char **argv)
{
	int		ctl;
	int		opt;
	nbd_cmd_t	cmd;

	while ((opt = getopt_clip(argc, argv, "adln:s:", nbdadm_options, NULL)) != -1) {
		switch (opt) {
		case 'a':
			attach = 1;
			break;
		case 'd':
			detach = 1;
			break;
		case 'l':
			list = 1;
			break;
		case 'n':
			printf("name=%s\n", optarg);
			name = strdup(optarg);
			break;
		case 's':
			printf("server=%s\n", optarg);
			server = getserveraddr(optarg);
			break;
		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}

	if ((attach + detach + list) != 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	ctl = open(NBDCTL_DEV, O_RDWR);

	if (ctl == -1) {
		perror("nbdadm");
		exit(EXIT_FAILURE);
	}

	if (list) {
		printf("Listing\n");
	} else if (attach) {
		snprintf(cmd.name, 1024, name);
		cmd.sin = server;
		printf("Attaching NBD device 1 (%s)\n", cmd.name);
		if (ioctl(ctl, NBD_ATTACH_DEV, &cmd) == -1) {
			perror("nbadm attach dev");
			exit(EXIT_FAILURE);
		}
	} else if (detach) {
		snprintf(cmd.name, 1024, name);
		printf("Detaching NBD device 1 (%s)\n", cmd.name);
		if (ioctl(ctl, NBD_DETACH_DEV, &cmd) == -1) {
			perror("nbadm detach dev");
			exit(EXIT_FAILURE);
		}
	}

	(void) close(ctl);
	exit(EXIT_SUCCESS);
}
