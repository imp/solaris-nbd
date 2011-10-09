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
#include <sys/blkdev.h>
#include <sys/cmn_err.h>
#include <sys/note.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/ksocket.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "nbd.h"
#include "nbd_impl.h"
#include "nbd_protocol.h"

#define	NBD_CTL_INSTANCE	0
#define	NBD_IS_CTL_INSTANCE(x)	(x == NBD_CTL_INSTANCE)

static nbd_ctl_state_t csp;

static ddi_dma_attr_t nbd_dma_attr = {
	.dma_attr_version       = DMA_ATTR_V0,
	.dma_attr_addr_lo       = 0x0000000000000000,
	.dma_attr_addr_hi       = 0xFFFFFFFFFFFFFFFF,
	.dma_attr_count_max     = 1,
	.dma_attr_align         = 4096,
	.dma_attr_burstsizes    = 0,
	.dma_attr_minxfer       = 512,                  /* 1 sector min */
	.dma_attr_maxxfer       = 512 * 2048,           /* 1 MB max */
	.dma_attr_seg           = 512 * 2048 - 1,
	.dma_attr_sgllen        = -1,
	.dma_attr_granular      = 512,
	.dma_attr_flags         = 0
};


static void
nbd_drive_info(void *pp, bd_drive_t *bdp)
{
	nbd_state_t	*sp	= (nbd_state_t *)pp;

	bdp->d_qsize		= 1;
	bdp->d_maxxfer		= 1024 * 1024;
	bdp->d_removable	= B_FALSE;
	bdp->d_hotpluggable	= B_TRUE;
	bdp->d_target		= sp->target;
#if defined(LUN_SUPPORT)
	bdp->d_lun		= sp->target;
#endif
}


static int
nbd_media_info(void *pp, bd_media_t *bmp)
{
	nbd_state_t	*sp	= (nbd_state_t *)pp;

	bmp->m_nblks	= 1024 * 1024 * 1024;
	bmp->m_blksize	= DEV_BSIZE;
	bmp->m_readonly	= B_FALSE;

	return (0);
}


static int
nbd_devid_init(void *pp, dev_info_t *dip, ddi_devid_t *didp)
{
	nbd_state_t	*sp	= (nbd_state_t *)pp;
	
	return (DDI_FAILURE);
}


static int
nbd_sync_cache(void *pp, bd_xfer_t *xp)
{
	nbd_state_t	*sp	= (nbd_state_t *)pp;
	
	return (EIO);
}


static int
nbd_bd_read(void *pp, bd_xfer_t *xp)
{
	nbd_state_t	*sp	= (nbd_state_t *)pp;
	
	return (EIO);
}


static int
nbd_bd_write(void *pp, bd_xfer_t *xp)
{
	nbd_state_t	*sp	= (nbd_state_t *)pp;
	
	return (EIO);
}


#if defined(DUMP_SUPPORT)
/* Not supported */
static int
nbd_bd_dump(void *private, bd_xfer_t *xp)
{
	return (0);
}
#endif


static bd_ops_t nbd_bd_ops = {
	.o_version	= BD_OPS_VERSION_0,
	.o_drive_info	= nbd_drive_info,
	.o_media_info	= nbd_media_info,
	.o_devid_init	= nbd_devid_init,
	.o_sync_cache	= nbd_sync_cache,
	.o_read		= nbd_bd_read,
	.o_write	= nbd_bd_write,
#if defined(DUMP_SUPPORT)
	.o_dump		= NULL
#endif
};


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
nbd_attach_dev(int instance)
{
	int		rc;
	nbd_state_t	*sp;
	
	rc = nbd_alloc_dev(instance);
	if (rc != DDI_SUCCESS) {
		return (rc);
	}

	cmn_err(CE_CONT, "nbd_attach_dev(), instance=%d\n", instance);

	sp = nbd_instance_to_state(instance);
	sp->bdh = bd_alloc_handle(sp, &nbd_bd_ops, &nbd_dma_attr, KM_SLEEP);
	if (sp->bdh == NULL) {
		nbd_free_dev(instance);
		return (DDI_FAILURE);
	}

	rc = bd_attach_handle(csp.dip, sp->bdh);
	if (rc != DDI_SUCCESS) {
		bd_free_handle(sp->bdh);
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

	cmn_err(CE_CONT, "nbd_detach_dev(), instance=%d\n", instance);

	rc = bd_detach_handle(sp->bdh);
	if (rc != DDI_SUCCESS) {
		return (rc);
	}
	bd_free_handle(sp->bdh);
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
	int	instance = getminor(dev);
	int	rc;

	cmn_err(CE_CONT, "nbd_ioctl(), instance=%d\n", instance);

	if (!NBD_IS_CTL_INSTANCE(instance)) {
		return (EINVAL);
	}

	switch (cmd) {
	case NBD_ATTACH_DEV:
		if (nbd_attach_dev(0) != DDI_SUCCESS) {
			rc = EINVAL;
		}
		break;
	case NBD_DETACH_DEV:
		if (nbd_detach_dev(0) != DDI_SUCCESS) {
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

	ddi_remove_minor_node(dip, 0);

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

	bd_mod_init(&nbd_devops);

	error = mod_install(&nbd_modlinkage);
	if (error != 0) {
		bd_mod_fini(&nbd_devops);
	}
	return (error);
}

int
_fini(void)
{
	int error;

	error = mod_remove(&nbd_modlinkage);
	if (error == 0) {
		bd_mod_fini(&nbd_devops);
	}
	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&nbd_modlinkage, modinfop));
}
