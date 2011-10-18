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
 * Solaris native NBD (Network Block Device) driver
 */

#include <sys/types.h>
#include <sys/ksynch.h>
#include <sys/cmn_err.h>
#include <sys/note.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/refstr.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
/* sys/ksocket.h relies on sys/sunddi.h */
#include <sys/socket.h>
#include <sys/ksocket.h>

#include "nbd.h"
#include "nbd_impl.h"
#include "nbd_protocol.h"

#define	NBD_CTL_INSTANCE	0
#define	NBD_IS_CTL_INSTANCE(x)	(x == NBD_CTL_INSTANCE)

static nbd_ctl_state_t csp;


static int
nbd_connect(nbd_state_t *sp)
{
	int	rc = 0;

	rc = ksocket_socket(&sp->sock, AF_INET, SOCK_STREAM, 0, KSOCKET_SLEEP, CRED());
	if (rc != 0) {
		return (DDI_FAILURE);
	}

	rc = ksocket_connect(sp->sock, &sp->addr.sin, SIZEOF_SOCKADDR(&sp->addr.sin), CRED());

	if (rc != 0) {
		ksocket_close(sp->sock, CRED());
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}


static void
nbd_disconnect(nbd_state_t *sp)
{
	ksocket_close(sp->sock, CRED());
}


static int
nbd_negotiate(nbd_state_t *sp)
{
	return (DDI_SUCCESS);
}


static int
nbd_alloc_dev(int instance)
{
	int		rc = DDI_FAILURE;
	nbd_state_t	*sp;

	if (csp.nbds[instance] == NULL) {	
		cmn_err(CE_CONT, "nbd_alloc_dev(), instance=%d\n", instance);
		sp = kmem_zalloc(sizeof (nbd_state_t), KM_SLEEP);
		sp->target = instance;
		csp.nbds[instance] = sp;
		cmn_err(CE_CONT, "nbd_alloc_dev(), csp.nbds[%d] = %p\n",
		    instance, csp.nbds[instance]);
		rc = DDI_SUCCESS;
	}

	return (rc);
}


static void
nbd_free_dev(int instance)
{
	nbd_state_t *sp;

	sp = csp.nbds[instance];
	csp.nbds[instance] = NULL;
	kmem_free(sp, sizeof (nbd_state_t));
}


static nbd_state_t *
nbd_instance_to_state(int instance)
{
	cmn_err(CE_CONT, "nbd_instance_to_state(), csp.nbds[%d] = %p\n",
	    instance, csp.nbds[instance]);
	return (csp.nbds[instance]);
}


static int
nbd_attach_dev(int instance, char *name, nbd_sockaddr_t *addr)
{
	int		rc;
	nbd_state_t	*sp;
	
	rc = nbd_alloc_dev(instance);
	if (rc != DDI_SUCCESS) {
		return (rc);
	}

	cmn_err(CE_CONT, "nbd_attach_dev(%d, %s)\n", instance, name);

	sp = nbd_instance_to_state(instance);

	sp->name = refstr_alloc(name);
	bcopy(addr, &sp->addr, sizeof (nbd_sockaddr_t));

	rc = ddi_create_minor_node(csp.dip, NBD_INSTANCE_NAME(sp),
	    S_IFCHR, instance, DDI_PSEUDO, 0);

	if (rc != DDI_SUCCESS) {
		refstr_rele(sp->name);
		nbd_free_dev(instance);
	}

	return (rc);
}


static int
nbd_detach_dev(int instance)
{
	nbd_state_t	*sp;
	int		rc;

	sp = nbd_instance_to_state(instance);
	if (sp == NULL) {
		return (DDI_FAILURE);
	}

	cmn_err(CE_CONT, "nbd_detach_dev(%d)\n", instance);

	ddi_remove_minor_node(csp.dip, NBD_INSTANCE_NAME(sp));
	refstr_rele(sp->name);
	nbd_free_dev(instance);
	return (DDI_SUCCESS);
}


static int
nbd_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	int	instance = getminor(*devp);

	if (csp.dip == NULL) {
		return (ENXIO);
	}

	cmn_err(CE_CONT, "nbd_open(), instance=%d\n", instance);

	if (NBD_IS_CTL_INSTANCE(instance)) {
		return (0);
	} else {
		return (EINVAL);
	}
}


static int
nbd_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	int	instance = getminor(dev);

	cmn_err(CE_CONT, "nbd_close(), instance=%d\n", instance);

	if (NBD_IS_CTL_INSTANCE(instance)) {
		return (0);
	} else {
		return (EINVAL);
	}
}


static int
nbd_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *credp, int *rvalp)
{
	nbd_cmd_t	nbdcmd;
	nbd_sockaddr_t	addr;
	int		instance = getminor(dev);
	int		rc = 0;

	cmn_err(CE_CONT, "nbd_ioctl(), instance=%d\n", instance);

	if (!NBD_IS_CTL_INSTANCE(instance)) {
		return (EINVAL);
	}

	if (ddi_copyin((void *)arg, &nbdcmd, sizeof (nbd_cmd_t), mode) == -1) {
		return (EFAULT);
	}

	switch (cmd) {
	case NBD_ATTACH_DEV:
		bzero(&addr, sizeof (addr));
		if (nbdcmd.addr != NULL) {
			if (ddi_copyin((void *)nbdcmd.addr, &addr,
			    sizeof (struct sockaddr), mode) == -1) {
				return (EFAULT);
			}
			if (ddi_copyin((void *)nbdcmd.addr, &addr,
			    SIZEOF_SOCKADDR(&addr.sin), mode) == -1) {
				return (EFAULT);
			}
		} else {
			return (EINVAL);
		}

		if (nbd_attach_dev(1, nbdcmd.name, &addr) != DDI_SUCCESS) {
			rc = EINVAL;
		}
		break;
	case NBD_DETACH_DEV:
		if (nbd_detach_dev(1) != DDI_SUCCESS) {
			rc = EINVAL;
		}
		break;
	default:
		rc = ENOTTY;
		break;
	}

	return (rc);
}


static struct cb_ops nbd_cb_ops = {
	.cb_open	= nbd_open,
	.cb_close	= nbd_close,
	.cb_strategy	= nodev,
	.cb_print	= nodev,
	.cb_dump	= nodev,
	.cb_read	= nodev,
	.cb_write	= nodev,
	.cb_ioctl	= nbd_ioctl,
	.cb_devmap	= nodev,
	.cb_mmap	= nodev,
	.cb_segmap	= nodev,
	.cb_chpoll	= nochpoll,
	.cb_prop_op	= ddi_prop_op,
	.cb_str		= NULL,
	.cb_flag	= D_NEW | D_MP | D_64BIT,
	.cb_rev		= CB_REV,
	.cb_aread	= nodev,
	.cb_awrite	= nodev
};


static int
nbd_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int		instance;
	int		rc;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
	default:
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(dip);
	if (!NBD_IS_CTL_INSTANCE(instance)) {
		return (DDI_FAILURE);
	}

	csp.dip = dip;

	rc = ddi_create_minor_node(dip, "ctl", S_IFCHR,
	    instance, DDI_PSEUDO, 0);

	if (rc != DDI_SUCCESS) {
		cmn_err(CE_WARN, "nbd_attach: failed to create minor node");
		return (DDI_FAILURE);
	}

	ddi_report_dev(dip);

	return (DDI_SUCCESS);
}


static int
nbd_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int		instance;

	cmn_err(CE_CONT, "nbd_detach(%d)\n", cmd);

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
	default:
		return (DDI_FAILURE);
	}

	cmn_err(CE_CONT, "nbd_detach(%d), detaching\n", cmd);

	instance = ddi_get_instance(dip);
	if (!NBD_IS_CTL_INSTANCE(instance)) {
		return (DDI_FAILURE);
	}

	/* Verify there are no NBD devices left */
	for (int i = 0; i < NBD_MAX_DEVICES; i++) {
		if (csp.nbds[i] != NULL) {
			return (DDI_FAILURE);
		}
	}

	ddi_remove_minor_node(dip, NULL);

	csp.dip = NULL;

	return (DDI_SUCCESS);
}


static int
nbd_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **rp)
{
	int	instance;
	int	rc		= DDI_FAILURE;

	instance = getminor((dev_t)arg);

	if (NBD_IS_CTL_INSTANCE(instance)) {
		switch (cmd) {
		case DDI_INFO_DEVT2DEVINFO:
			if (csp.dip != NULL) {
				*rp = csp.dip;
				rc = DDI_SUCCESS;
			}
			break;
		case DDI_INFO_DEVT2INSTANCE:
			*rp = (void *)(uintptr_t)instance;
			rc = DDI_SUCCESS;
			break;
		}
	}

	return (rc);
}


static struct dev_ops nbd_devops = {
	.devo_rev	= DEVO_REV,
	.devo_refcnt	= 0,
	.devo_getinfo	= nbd_getinfo,
	.devo_identify	= nulldev,
	.devo_probe	= nulldev,
	.devo_attach	= nbd_attach,
	.devo_detach	= nbd_detach,
	.devo_reset	= nodev,
	.devo_cb_ops	= &nbd_cb_ops,
	.devo_bus_ops	= NULL,
	.devo_power	= NULL,
	.devo_quiesce	= ddi_quiesce_not_supported
};


static struct modldrv nbd_modldrv = {
	.drv_modops	= &mod_driverops,
	.drv_linkinfo	= "Solaris NBD driver v0",
	.drv_dev_ops	= &nbd_devops
};


static struct modlinkage nbd_modlinkage = {
	.ml_rev		= MODREV_1,
	.ml_linkage	= {&nbd_modldrv, NULL, NULL, NULL}
};


/*
 * Loadable module entry points.
 */
int
_init(void)
{
	int error;

	error = mod_install(&nbd_modlinkage);
	return (error);
}

int
_fini(void)
{
	int error;

	error = mod_remove(&nbd_modlinkage);
	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&nbd_modlinkage, modinfop));
}
