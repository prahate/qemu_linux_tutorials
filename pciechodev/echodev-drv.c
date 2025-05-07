#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>

#define MAJDEV 75
#define DEVNAME "echodev"

#define VID 0x1234
#define DID 0xdeed


struct echodev {
	struct pci_dev *pdev;
} mydev;

static int echo_mmap(struct file *file, struct vm_area_struct *vma)
{
	int status;

	vma->vm_pgoff = pci_resource_start(mydev.pdev, 1) >> PAGE_SHIFT;

	status = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start,
		       vma->vm_page_prot);

	if (status)
	{
		pr_info("echodev-drv: error mapping memory\n");
		return -status;
	}
	
	return 0;
}

static struct file_operations fops = {
	.mmap = echo_mmap,
};

static struct pci_device_id echo_ids[] = {
	{PCI_DEVICE(VID,DID)},
	{},
};

MODULE_DEVICE_TABLE(pci, echo_ids);

static int echo_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int status;
	void __iomem *ptr_bar0, __iomem *ptr_bar1;

	status = register_chrdev(MAJDEV, DEVNAME, &fops);
	if (status < 0) {
		pr_err("echodev-drv: error allocating device numer\n");
		goto fdev;
	}

	status = pcim_enable_device(pdev);
	if (status) {
		pr_err("echodev-drv: error enabling device\n");
		return status;
	}
	
	ptr_bar0 = pcim_iomap(pdev, 0, pci_resource_len(pdev, 0));
	if (!ptr_bar0) {
		pr_err("echodev-drv: error mapping BAR0\n");
		return -ENODEV;
	}
	
	ptr_bar1 = pcim_iomap(pdev, 1, pci_resource_len(pdev, 1));
	if (!ptr_bar1) {
		pr_err("echodev-drv: error mapping BAR1\n");
		return -ENODEV;
	}

	pr_info("echodev-drv: ID Reg 0x%x\n", ioread32(ptr_bar0));
	pr_info("echodev-drv: Random Reg 0x%x\n", ioread32(ptr_bar0 + 0xc));

	iowrite32(0x11223344, ptr_bar0 + 0x4);
	mdelay(1);
	pr_info("echodev-drv: INV val 0x%x\n", ioread32(ptr_bar0 + 0x4));

	iowrite32(0x44332211, ptr_bar1);
	pr_info("echodev-drv: reading 1 byte 0x%x\n", ioread8(ptr_bar1));
	pr_info("echodev-drv: reading 2 byte 0x%x\n", ioread16(ptr_bar1));
	pr_info("echodev-drv: reading 4 byte 0x%x\n", ioread32(ptr_bar1));

	return 0;

fdev:
	unregister_chrdev(MAJDEV, DEVNAME);
	return status;
}


static void echo_remove(struct pci_dev *pdev)
{
	pr_info("echodev-drv: removing device\n");
	unregister_chrdev(MAJDEV, DEVNAME);
}


static struct pci_driver echo_driver = {
	.name = "echodev-drv",
	.probe = echo_probe,
	.remove = echo_remove,
	.id_table = echo_ids,
};

module_pci_driver(echo_driver);

MODULE_LICENSE("GPL");

