/*
 * Copyright (C) 2012 Red Hat, Inc.  All rights reserved.
 *     Author: Alex Williamson <alex.williamson@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Derived from original vfio:
 * Copyright 2010 Cisco Systems, Inc.  All rights reserved.
 * Author: Tom Lyon, pugs@cisco.com
 */

#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/irqbypass.h>
#include <linux/types.h>

#ifndef VFIO_PCI_PRIVATE_H
#define VFIO_PCI_PRIVATE_H

#define VFIO_PCI_OFFSET_SHIFT   40

#define VFIO_PCI_OFFSET_TO_INDEX(off)	(off >> VFIO_PCI_OFFSET_SHIFT)
#define VFIO_PCI_INDEX_TO_OFFSET(index)	((u64)(index) << VFIO_PCI_OFFSET_SHIFT)
#define VFIO_PCI_OFFSET_MASK	(((u64)(1) << VFIO_PCI_OFFSET_SHIFT) - 1)

/* Special capability IDs predefined access */
#define PCI_CAP_ID_INVALID		0xFF	/* default raw access */
#define PCI_CAP_ID_INVALID_VIRT		0xFE	/* default virt access */

/* Cap maximum number of ioeventfds per device (arbitrary) */
#define VFIO_PCI_IOEVENTFD_MAX		1000

struct vfio_pci_ioeventfd {
	struct list_head	next;
	struct virqfd		*virqfd;
	void __iomem		*addr;
	uint64_t		data;
	loff_t			pos;
	int			bar;
	int			count;
};

struct vfio_pci_irq_ctx {
	struct eventfd_ctx	*trigger;
	struct virqfd		*unmask;
	struct virqfd		*mask;
	char			*name;
	bool			masked;
	struct irq_bypass_producer	producer;
};

struct vfio_pci_device;
struct vfio_pci_region;

struct vfio_pci_regops {
	size_t	(*rw)(struct vfio_pci_device *vdev, char __user *buf,
		      size_t count, loff_t *ppos, bool iswrite);
	void	(*release)(struct vfio_pci_device *vdev,
			   struct vfio_pci_region *region);
	int	(*mmap)(struct vfio_pci_device *vdev,
			struct vfio_pci_region *region,
			struct vm_area_struct *vma);
	int	(*add_capability)(struct vfio_pci_device *vdev,
				  struct vfio_pci_region *region,
				  struct vfio_info_cap *caps);
};

struct vfio_pci_region {
	u32				type;
	u32				subtype;
	const struct vfio_pci_regops	*ops;
	void				*data;
	size_t				size;
	u32				flags;
};

struct vfio_pci_dummy_resource {
	struct resource		resource;
	int			index;
	struct list_head	res_next;
};

struct vfio_pci_reflck {
	struct kref		kref;
	struct mutex		lock;
};

struct vfio_pci_device {
	struct pci_dev		*pdev;
	void __iomem		*barmap[PCI_STD_RESOURCE_END + 1];
	bool			bar_mmap_supported[PCI_STD_RESOURCE_END + 1];
	u8			*pci_config_map;
	u8			*vconfig;
	struct perm_bits	*msi_perm;
	spinlock_t		irqlock;
	struct mutex		igate;
	struct vfio_pci_irq_ctx	*ctx;
	int			num_ctx;
	int			irq_type;
	int			num_regions;
	struct vfio_pci_region	*region;
	u8			msi_qmax;
	u8			msix_bar;
	u16			msix_size;
	u32			msix_offset;
	u32			rbar[7];
	bool			pci_2_3;
	bool			virq_disabled;
	bool			reset_works;
	bool			extended_caps;
	bool			bardirty;
	bool			has_vga;
	bool			needs_reset;
	bool			nointx;
	bool			needs_pm_restore;
	struct pci_saved_state	*pci_saved_state;
	struct pci_saved_state	*pm_save;
	struct vfio_pci_reflck	*reflck;
	int			refcnt;
	int			ioeventfds_nr;
	struct eventfd_ctx	*err_trigger;
	struct eventfd_ctx	*req_trigger;
	struct list_head	dummy_resources_list;
	struct mutex		ioeventfds_lock;
	struct list_head	ioeventfds_list;
	bool			nointxmask;
#ifdef CONFIG_VFIO_PCI_VGA
	bool			disable_vga;
#endif
	bool			disable_idle_d3;
};

#define is_intx(vdev) (vdev->irq_type == VFIO_PCI_INTX_IRQ_INDEX)
#define is_msi(vdev) (vdev->irq_type == VFIO_PCI_MSI_IRQ_INDEX)
#define is_msix(vdev) (vdev->irq_type == VFIO_PCI_MSIX_IRQ_INDEX)
#define is_irq_none(vdev) (!(is_intx(vdev) || is_msi(vdev) || is_msix(vdev)))
#define irq_is(vdev, type) (vdev->irq_type == type)

extern const struct pci_error_handlers vfio_err_handlers;

static inline bool vfio_pci_is_vga(struct pci_dev *pdev)
{
	return (pdev->class >> 8) == PCI_CLASS_DISPLAY_VGA;
}

static inline bool vfio_vga_disabled(struct vfio_pci_device *vdev)
{
#ifdef CONFIG_VFIO_PCI_VGA
	return vdev->disable_vga;
#else
	return true;
#endif
}

extern void vfio_pci_intx_mask(struct vfio_pci_device *vdev);
extern void vfio_pci_intx_unmask(struct vfio_pci_device *vdev);

extern int vfio_pci_set_irqs_ioctl(struct vfio_pci_device *vdev,
				   uint32_t flags, unsigned index,
				   unsigned start, unsigned count, void *data);

extern ssize_t vfio_pci_config_rw(struct vfio_pci_device *vdev,
				  char __user *buf, size_t count,
				  loff_t *ppos, bool iswrite);

extern ssize_t vfio_pci_bar_rw(struct vfio_pci_device *vdev, char __user *buf,
			       size_t count, loff_t *ppos, bool iswrite);

extern ssize_t vfio_pci_vga_rw(struct vfio_pci_device *vdev, char __user *buf,
			       size_t count, loff_t *ppos, bool iswrite);

extern long vfio_pci_ioeventfd(struct vfio_pci_device *vdev, loff_t offset,
			       uint64_t data, int count, int fd);

extern int vfio_pci_init_perm_bits(void);
extern void vfio_pci_uninit_perm_bits(void);

extern int vfio_config_init(struct vfio_pci_device *vdev);
extern void vfio_config_free(struct vfio_pci_device *vdev);

extern int vfio_pci_register_dev_region(struct vfio_pci_device *vdev,
					unsigned int type, unsigned int subtype,
					const struct vfio_pci_regops *ops,
					size_t size, u32 flags, void *data);

extern int vfio_pci_set_power_state(struct vfio_pci_device *vdev,
				    pci_power_t state);
extern unsigned int vfio_pci_set_vga_decode(void *opaque, bool single_vga);
extern int vfio_pci_enable(struct vfio_pci_device *vdev);
extern void vfio_pci_disable(struct vfio_pci_device *vdev);
extern long vfio_pci_ioctl(void *device_data,
			unsigned int cmd, unsigned long arg);
extern ssize_t vfio_pci_read(void *device_data, char __user *buf,
			size_t count, loff_t *ppos);
extern ssize_t vfio_pci_write(void *device_data, const char __user *buf,
			size_t count, loff_t *ppos);
extern int vfio_pci_mmap(void *device_data, struct vm_area_struct *vma);
extern void vfio_pci_request(void *device_data, unsigned int count);
extern void vfio_pci_fill_ids(char *ids, struct pci_driver *driver);
extern int vfio_pci_reflck_attach(struct vfio_pci_device *vdev);
extern void vfio_pci_reflck_put(struct vfio_pci_reflck *reflck);
extern void vfio_pci_probe_power_state(struct vfio_pci_device *vdev);

#ifdef CONFIG_VFIO_PCI_IGD
extern int vfio_pci_igd_init(struct vfio_pci_device *vdev);
#else
static inline int vfio_pci_igd_init(struct vfio_pci_device *vdev)
{
	return -ENODEV;
}
#endif
#ifdef CONFIG_VFIO_PCI_NVLINK2
extern int vfio_pci_nvdia_v100_nvlink2_init(struct vfio_pci_device *vdev);
extern int vfio_pci_ibm_npu2_init(struct vfio_pci_device *vdev);
#else
static inline int vfio_pci_nvdia_v100_nvlink2_init(struct vfio_pci_device *vdev)
{
	return -ENODEV;
}

static inline int vfio_pci_ibm_npu2_init(struct vfio_pci_device *vdev)
{
	return -ENODEV;
}
#endif
#endif /* VFIO_PCI_PRIVATE_H */
