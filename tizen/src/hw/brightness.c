/* 
 * Qemu brightness emulator
 *
 * Copyright (C) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: 
 * DoHyung Hong <don.hong@samsung.com>
 * SeokYeon Hwang <syeon.hwang@samsung.com>
 * Hyunjun Son <hj79.son@samsung.com>
 * SangJin Kim <sangjin3.kim@samsung.com>
 * MunKyu Im <munkyu.im@samsung.com>
 * KiTae Kim <kt920.kim@samsung.com>
 * JinHyung Jo <jinhyung.jo@samsung.com>
 * SungMin Ha <sungmin82.ha@samsung.com>
 * JiHye Kim <jihye1128.kim@samsung.com>
 * GiWoong Kim <giwoong.kim@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 * DongKyun Yun <dk77.yun@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */


#include "pc.h"
#include "pci.h"
#include "pci_ids.h"

#define QEMU_DEV_NAME                       "brightness"

#define BRIGHTNESS_MEM_SIZE         (4 * 1024)      /* 4KB */
#define BRIGHTNESS_REG_SIZE         256

#define BRIGHTNESS_MIN              (0)
#define BRIGHTNESS_MAX              (24)

typedef struct BrightnessState {
    PCIDevice dev;
    
    ram_addr_t  offset;
    int brightness_mmio_io_addr;

    uint32_t ioport_addr;   // guest addr
    uint32_t mmio_addr;     // guest addr
} BrightnessState;

enum {
    BRIGHTNESS_LEVEL    = 0x00,
    BRIGHTNESS_OFF    = 0x04,
};

//uint8_t* brightness_ptr;  // pointer in qemu space
uint32_t brightness_level = 24;
uint32_t brightness_off = 0;

//#define DEBUG_BRIGHTNESS

#if defined (DEBUG_BRIGHTNESS)
#  define DEBUG_PRINT(x) do { printf x ; } while (0)
#else
#  define DEBUG_PRINT(x)
#endif

static uint32_t brightness_reg_read(void *opaque, target_phys_addr_t addr)
{
    switch (addr & 0xFF) {
    case BRIGHTNESS_LEVEL:
        DEBUG_PRINT(("brightness_reg_read: brightness_level = %d\n", brightness_level));
        return brightness_level;

    default:
        fprintf(stderr, "wrong brightness register read - addr : %d\n", addr);
    }

    return 0;
}

static void brightness_reg_write(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    DEBUG_PRINT(("brightness_reg_write: addr = %d, val = %d\n", addr, val));

#if BRIGHTNESS_MIN > 0
    if (val < BRIGHTNESS_MIN || val > BRIGHTNESS_MAX) {
#else
    if (val > BRIGHTNESS_MAX) {
#endif
        fprintf(stderr, "brightness_reg_write: Invalide brightness level.\n");
    }

    switch (addr & 0xFF) {
    case BRIGHTNESS_LEVEL:
        brightness_level = val;
        return;
    case BRIGHTNESS_OFF:
    	DEBUG_PRINT(("Brightness off : %d\n", val));
    	brightness_off = val;
    	return;
    default:
        fprintf(stderr, "wrong brightness register write - addr : %d\n", addr);
    }
}

static CPUReadMemoryFunc * const brightness_reg_readfn[3] = {
    brightness_reg_read,
    brightness_reg_read,
    brightness_reg_read,
};

static CPUWriteMemoryFunc * const brightness_reg_writefn[3] = {
    brightness_reg_write,
    brightness_reg_write,
    brightness_reg_write,
};

#if 0
static void brightness_ioport_map(PCIDevice *dev, int region_num,
                   pcibus_t addr, pcibus_t size, int type)
{
    //BrightnessState *s = DO_UPCAST(BrightnessState, dev, dev);

    //cpu_register_physical_memory(addr, BRIGHTNESS_MEM_SIZE,
    //           s->offset);

    //s->ioport_addr = addr;
}
#endif

static void brightness_mmio_map(PCIDevice *dev, int region_num,
                   pcibus_t addr, pcibus_t size, int type)
{
    BrightnessState *s = DO_UPCAST(BrightnessState, dev, dev);
    
    cpu_register_physical_memory(addr, BRIGHTNESS_REG_SIZE,
                                 s->brightness_mmio_io_addr);
    
    s->mmio_addr = addr;
}

static int brightness_initfn(PCIDevice *dev)
{
    BrightnessState *s = DO_UPCAST(BrightnessState, dev, dev);
    uint8_t *pci_conf = s->dev.config;

    pci_config_set_vendor_id(pci_conf, PCI_VENDOR_ID_TIZEN);
    pci_config_set_device_id(pci_conf, PCI_DEVICE_ID_VIRTUAL_BRIGHTNESS);
    pci_config_set_class(pci_conf, PCI_CLASS_DISPLAY_OTHER);

    //s->offset = qemu_ram_alloc(NULL, "brightness", BRIGHTNESS_MEM_SIZE);
    //brightness_ptr = qemu_get_ram_ptr(s->offset);

    s->brightness_mmio_io_addr = cpu_register_io_memory(brightness_reg_readfn,
                                             brightness_reg_writefn, s,
                                             DEVICE_LITTLE_ENDIAN);
 
    /* setup memory space */
    /* memory #0 device memory (overlay surface) */
    /* memory #1 memory-mapped I/O */
    //pci_register_bar(&s->dev, 0, BRIGHTNESS_MEM_SIZE,
    //      PCI_BASE_ADDRESS_SPACE_IO, brightness_ioport_map);

    pci_register_bar(&s->dev, 1, BRIGHTNESS_REG_SIZE,
                     PCI_BASE_ADDRESS_SPACE_MEMORY, brightness_mmio_map);

    return 0;
}

/* external interface */
int pci_get_brightness(void)
{
    return brightness_level;
}

int pci_brightness_init(PCIBus *bus)
{
    pci_create_simple(bus, -1, QEMU_DEV_NAME);
    return 0;
}

static PCIDeviceInfo brightness_info = {
    .qdev.name    = QEMU_DEV_NAME,
    .qdev.size    = sizeof(BrightnessState),
    .no_hotplug   = 1,
    .init         = brightness_initfn,
};

static void brightness_register(void)
{
    pci_qdev_register(&brightness_info);
}

device_init(brightness_register);
