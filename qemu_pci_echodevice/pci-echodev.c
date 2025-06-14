#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/pci/pci.h"
#include "hw/hw.h"
#include "hw/pci/msi.h"
#include "qemu/timer.h"
#include "qom/object.h"
#include "qemu/main-loop.h" /* iothread mutex */
#include "qemu/module.h"
#include "qapi/visitor.h"

#define TYPE_PCI_CUSTOM_DEVICE "pci-echodev"

#define ID_REGISTER 0x0
#define INV_REGISTER 0x4
#define IRQ_REGISTER 0x8
#define RANDVAL_REGISTER 0xC


typedef struct sPciechodevState PciechodevState;

//This macro provides the instance type cast functions for a QOM type.
DECLARE_INSTANCE_CHECKER(PciechodevState, PCIECHODEV, TYPE_PCI_CUSTOM_DEVICE)

//struct defining/descring the state
//of the custom pci device.
struct sPciechodevState {
    PCIDevice pdev;
    MemoryRegion mmio_bar0;
    MemoryRegion mmio_bar1;
    uint32_t bar0[4];
    uint8_t  bar1[4096];
};


static uint64_t pciechodev_bar0_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
	PciechodevState *pciechodev = opaque;
	printf("PCIECHODEV: pciechodev_bar0_mmio_read() addr %lx, size %x \n", addr, size);

	if (addr == RANDVAL_REGISTER)
		return rand();

	return pciechodev->bar0[addr/4];
}


static void pciechodev_bar0_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                unsigned size)
{
    printf("PCIECHODEV: pciechodev_bar0_mmio_write()  addr %lx, val %lx \n", addr, val);
    PciechodevState *pciechodev = opaque;

    switch (addr) {
    case ID_REGISTER:
	break;
    case INV_REGISTER:
	pciechodev->bar0[1] = ~val;
	break;
    case RANDVAL_REGISTER:
	break;
    case IRQ_REGISTER:
	break;
    }
}

///ops for the Memory Region BAR0.
static const MemoryRegionOps pciechodev_bar0_mmio_ops = {
    .read = pciechodev_bar0_mmio_read,
    .write = pciechodev_bar0_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },

};


static uint64_t pciechodev_bar1_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    printf("PCIECHODEV: pciechodev_bar1_mmio_read()  addr %lx, size %x \n", addr, size);
    PciechodevState *pciechodev = opaque;

    if (size == 1)
	return pciechodev->bar1[addr];
    else if (size == 2) {
	uint16_t *ptr = (uint16_t *) &pciechodev->bar1[addr];
  	return *ptr;
    }
    else if (size == 4) {
	uint32_t *ptr = (uint32_t *) &pciechodev->bar1[addr];
  	return *ptr;
    }
    else if (size == 8) {
	uint64_t *ptr = (uint64_t *) &pciechodev->bar1[addr];
  	return *ptr;
    }

    return 0xfffffffffffffffA;

}

static void pciechodev_bar1_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    printf("PCIECHODEV: pciechodev_bar1_mmio_write()  addr %lx, val %lx \n", addr, val);
    PciechodevState *pciechodev = opaque;
    
    if (size == 1)
	pciechodev->bar1[addr] = (uint8_t)val;
    else if (size == 2) {
	uint16_t *ptr = (uint16_t *) &pciechodev->bar1[addr];
  	*ptr = (uint16_t)val;
    }
    else if (size == 4) {
	uint32_t *ptr = (uint32_t *) &pciechodev->bar1[addr];
  	*ptr = (uint32_t)val;
    }
    else if (size == 8) {
	uint64_t *ptr = (uint64_t *) &pciechodev->bar1[addr];
  	*ptr = (uint64_t)val;
    }
}

///ops for the Memory Region BAR1.
static const MemoryRegionOps pciechodev_bar1_mmio_ops = {
    .read = pciechodev_bar1_mmio_read,
    .write = pciechodev_bar1_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },

};

//implementation of the realize function.
static void pci_cpciechodev_realize(PCIDevice *pdev, Error **errp)
{
    PciechodevState *pciechodev = PCIECHODEV(pdev);
    uint8_t *pci_conf = pdev->config;

    pci_config_set_interrupt_pin(pci_conf, 1);
    //supporting msi vectors ( 1 msi vector here of 64bit).
    //Make PCI device @dev MSI-capable.
    /*
    * Non-zero @offset puts capability MSI at that offset in PCI config
	* space.
	* @nr_vectors is the number of MSI vectors (1, 2, 4, 8, 16 or 32).
	* If @msi64bit, make the device capable of sending a 64-bit message
	* address.
	* If @msi_per_vector_mask, make the device support per-vector masking.
	* @errp is for returning errors.
	* Return 0 on success; set @errp and return -errno on error. 
    if (msi_init(pdev, 0, 1, true, false, errp)) {
        return;
    }*/
	
    ///initial configuration of devices registers.
    memset(pciechodev->bar0, 0, 16);
    memset(pciechodev->bar1, 0, 4096);
    pciechodev->bar0[0] = 0xcafeaffe;

    // Initialize an I/O memory region(cpcidev->mmio). 
    // Accesses to this region will cause the callbacks 
    // of the cpcidev_mmio_ops to be called.
    memory_region_init_io(&pciechodev->mmio_bar0, OBJECT(pciechodev), &pciechodev_bar0_mmio_ops, pciechodev, "pciechodev0-mmio", 16);
    // registering the pdev and all of the above configuration 
    // (actually filling a PCI-IO region with our configuration.
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &pciechodev->mmio_bar0);
    
    // Initialize an I/O memory region(cpcidev->mmio). 
    // Accesses to this region will cause the callbacks 
    // of the cpcidev_mmio_ops to be called.
    memory_region_init_io(&pciechodev->mmio_bar1, OBJECT(pciechodev), &pciechodev_bar1_mmio_ops, pciechodev, "pciechodev1-mmio", 4096);
    // registering the pdev and all of the above configuration 
    // (actually filling a PCI-IO region with our configuration.
    pci_register_bar(pdev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &pciechodev->mmio_bar1);
}


// uninitializing functions performed.
static void pci_cpciechodev_uninit(PCIDevice *pdev)
{
    return;
}


///initialization of the device
static void pciechodev_instance_init(Object *obj)
{
    return ;
}

static void pciechodev_class_init(ObjectClass *class,  const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(class);
	
    //definition of realize func().
    k->realize = pci_cpciechodev_realize;
    //definition of uninit func().
    k->exit = pci_cpciechodev_uninit;
    k->vendor_id = PCI_VENDOR_ID_QEMU;
    k->device_id = 0xdeed; //our device id, 'abcd' hexadecimal
    k->revision = 0x10;
    k->class_id = PCI_CLASS_OTHERS;

    /**
    * set_bit - Set a bit in memory
    * @nr: the bit to set
    * @addr: the address to start counting from
    */
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static void pci_custom_device_register_types(void)
{
    static InterfaceInfo interfaces[] = {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    };
    static const TypeInfo custom_pci_device_info = {
        .name          = TYPE_PCI_CUSTOM_DEVICE,
        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(PciechodevState),
        .instance_init = pciechodev_instance_init,
        .class_init    = pciechodev_class_init,
        .interfaces = interfaces,
    };
    //registers the new type.
    type_register_static(&custom_pci_device_info);
}

type_init(pci_custom_device_register_types)
