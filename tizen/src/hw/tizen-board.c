/*
 * TIZEN base board
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact:
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 * DongKyun Yun <dk77.yun@samsung.com>
 * DoHyung Hong <don.hong@samsung.com>
 * SeokYeon Hwang <syeon.hwang@samsung.com>
 * Hyunjun Son <hj79.son@samsung.com>
 * SangJin Kim <sangjin3.kim@samsung.com>
 * KiTae Kim <kt920.kim@samsung.com>
 * JinHyung Jo <jinhyung.jo@samsung.com>
 * SungMin Ha <sungmin82.ha@samsung.com>
 * MunKyu Im <munkyu.im@samsung.com>
 * JiHye Kim <jihye1128.kim@samsung.com>
 * GiWoong Kim <giwoong.kim@samsung.com> 
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
 * x86 board from pc_piix.c...
 * add some TIZEN-speciaized device...
 */


#include "hw.h"
#include "pc.h"
#include "apic.h"
#include "pci.h"
#include "usb-uhci.h"
#include "usb-ohci.h"
#include "net.h"
#include "boards.h"
#include "ide.h"
#include "kvm.h"
#include "sysemu.h"
#include "sysbus.h"
#include "arch_init.h"
#include "blockdev.h"
#include "maru_pm.h"

#define MAX_IDE_BUS 2

static const int ide_iobase[MAX_IDE_BUS] = { 0x1f0, 0x170 };
static const int ide_iobase2[MAX_IDE_BUS] = { 0x3f6, 0x376 };
static const int ide_irq[MAX_IDE_BUS] = { 14, 15 };

static void ioapic_init(IsaIrqState *isa_irq_state)
{
    DeviceState *dev;
    SysBusDevice *d;
    unsigned int i;

    dev = qdev_create(NULL, "ioapic");
    qdev_init_nofail(dev);
    d = sysbus_from_qdev(dev);
    sysbus_mmio_map(d, 0, 0xfec00000);

    for (i = 0; i < IOAPIC_NUM_PINS; i++) {
        isa_irq_state->ioapic[i] = qdev_get_gpio_in(dev, i);
    }
}

static void tizen_x86_machine_init(ram_addr_t ram_size,
                        const char *boot_device,
                        const char *kernel_filename,
                        const char *kernel_cmdline,
                        const char *initrd_filename,
                        const char *cpu_model)
{
    int pci_enabled = 1;
    int i;
    ram_addr_t below_4g_mem_size, above_4g_mem_size;
    PCIBus *pci_bus;
    PCII440FXState *i440fx_state;
    int piix3_devfn = -1;
    qemu_irq *cpu_irq;
    qemu_irq *isa_irq;
    qemu_irq *i8259;
    qemu_irq *cmos_s3;
    qemu_irq *smi_irq;
    IsaIrqState *isa_irq_state;
    DriveInfo *hd[MAX_IDE_BUS * MAX_IDE_DEVS];
    FDCtrl *floppy_controller;
    BusState *idebus[MAX_IDE_BUS];
    ISADevice *rtc_state;

    pc_cpus_init(cpu_model);

    vmport_init();

    /* allocate ram and load rom/bios */
    pc_memory_init(ram_size, kernel_filename, kernel_cmdline, initrd_filename,
                   &below_4g_mem_size, &above_4g_mem_size);

    cpu_irq = pc_allocate_cpu_irq();
    i8259 = i8259_init(cpu_irq[0]);
    isa_irq_state = qemu_mallocz(sizeof(*isa_irq_state));
    isa_irq_state->i8259 = i8259;
    if (pci_enabled) {
        ioapic_init(isa_irq_state);
    }
    isa_irq = qemu_allocate_irqs(isa_irq_handler, isa_irq_state, 24);

    if (pci_enabled) {
        pci_bus = i440fx_init(&i440fx_state, &piix3_devfn, isa_irq, ram_size);
    } else {
        pci_bus = NULL;
        i440fx_state = NULL;
        isa_bus_new(NULL);
    }
    isa_bus_irqs(isa_irq);

    pc_register_ferr_irq(isa_get_irq(13));

    pc_vga_init(pci_enabled? pci_bus: NULL);

    /* init basic PC hardware */
    pc_basic_device_init(isa_irq, &floppy_controller, &rtc_state);

    for(i = 0; i < nb_nics; i++) {
        NICInfo *nd = &nd_table[i];

        if (!pci_enabled || (nd->model && strcmp(nd->model, "ne2k_isa") == 0))
            pc_init_ne2k_isa(nd);
        else
            pci_nic_init_nofail(nd, "e1000", NULL);
    }

    if (drive_get_max_bus(IF_IDE) >= MAX_IDE_BUS) {
        fprintf(stderr, "qemu: too many IDE bus\n");
        exit(1);
    }

    for(i = 0; i < MAX_IDE_BUS * MAX_IDE_DEVS; i++) {
        hd[i] = drive_get(IF_IDE, i / MAX_IDE_DEVS, i % MAX_IDE_DEVS);
    }

    if (pci_enabled) {
        PCIDevice *dev;
        dev = pci_piix3_ide_init(pci_bus, hd, piix3_devfn + 1);
        idebus[0] = qdev_get_child_bus(&dev->qdev, "ide.0");
        idebus[1] = qdev_get_child_bus(&dev->qdev, "ide.1");
    } else {
        for(i = 0; i < MAX_IDE_BUS; i++) {
            ISADevice *dev;
            dev = isa_ide_init(ide_iobase[i], ide_iobase2[i], ide_irq[i],
                               hd[MAX_IDE_DEVS * i], hd[MAX_IDE_DEVS * i + 1]);
            idebus[i] = qdev_get_child_bus(&dev->qdev, "ide.0");
        }
    }

// commented out by caramis... for use 'tizen-ac97'...
//    audio_init(isa_irq, pci_enabled ? pci_bus : NULL);

    pc_cmos_init(below_4g_mem_size, above_4g_mem_size, boot_device,
                 idebus[0], idebus[1], floppy_controller, rtc_state);

    if (pci_enabled && usb_enabled) {
        usb_uhci_piix3_init(pci_bus, piix3_devfn + 2);
    }

    if (pci_enabled && acpi_enabled) {
        uint8_t *eeprom_buf = qemu_mallocz(8 * 256); /* XXX: make this persistent */
        i2c_bus *smbus;

        cmos_s3 = qemu_allocate_irqs(pc_cmos_set_s3_resume, rtc_state, 1);
        smi_irq = qemu_allocate_irqs(pc_acpi_smi_interrupt, first_cpu, 1);
        /* TODO: Populate SPD eeprom data.  */

        // commented out for tizen acpi
//        smbus = piix4_pm_init(pci_bus, piix3_devfn + 3, 0xb100,
//                              isa_get_irq(9), *cmos_s3, *smi_irq,
//                              kvm_enabled());
        smbus = maru_pm_init(pci_bus, piix3_devfn + 3, 0xb100,
                              isa_get_irq(9), *cmos_s3, *smi_irq,
                              kvm_enabled());
        for (i = 0; i < 8; i++) {
            DeviceState *eeprom;
            eeprom = qdev_create((BusState *)smbus, "smbus-eeprom");
            qdev_prop_set_uint8(eeprom, "address", 0x50 + i);
            qdev_prop_set_ptr(eeprom, "data", eeprom_buf + (i * 256));
            qdev_init_nofail(eeprom);
        }
    }

    if (i440fx_state) {
        i440fx_init_memory_mappings(i440fx_state);
    }

    if (pci_enabled) {
        pc_pci_device_init(pci_bus);
    }

// TIZEN-specialized device init...
    if (pci_enabled) {
        svcamera_pci_init(pci_bus);
        tizen_ac97_init(pci_bus);
    }

#ifdef CONFIG_FFMPEG
    if (pci_enabled) {
        pci_codec_init(pci_bus);        
    }   
#endif
}

static void tizen_arm_machine_init(ram_addr_t ram_size,
                        const char *boot_device,
                        const char *kernel_filename,
                        const char *kernel_cmdline,
                        const char *initrd_filename,
                        const char *cpu_model)
{
}

static void tizen_common_init(ram_addr_t ram_size,
                        const char *boot_device,
                        const char *kernel_filename,
                        const char *kernel_cmdline,
                        const char *initrd_filename,
                        const char *cpu_model)
{
// prepare for universal virtual board...
#if defined(TARGET_I386)
#elif defined(TARGET_ARM)
#endif
}

static void tizen_x86_board_init(ram_addr_t ram_size,
                        const char *boot_device,
                        const char *kernel_filename,
                        const char *kernel_cmdline,
                        const char *initrd_filename,
                        const char *cpu_model)
{
    tizen_x86_machine_init(ram_size, boot_device, kernel_filename, 
                        kernel_cmdline, initrd_filename, cpu_model);
    tizen_common_init(ram_size, boot_device, kernel_filename, 
                        kernel_cmdline, initrd_filename, cpu_model);
}
static void tizen_arm_board_init(ram_addr_t ram_size,
                        const char *boot_device,
                        const char *kernel_filename,
                        const char *kernel_cmdline,
                        const char *initrd_filename,
                        const char *cpu_model)
{
    tizen_arm_machine_init(ram_size, boot_device, kernel_filename, 
                        kernel_cmdline, initrd_filename, cpu_model);
    tizen_common_init(ram_size, boot_device, kernel_filename, 
                        kernel_cmdline, initrd_filename, cpu_model);
}

static QEMUMachine tizen_x86_machine = {
    .name = "tizen-x86-machine",
    .desc = "TIZEN base board(x86)",
    .init = tizen_x86_board_init,
    .max_cpus = 255,
};

static QEMUMachine tizen_arm_machine = {
    .name = "tizen-arm-machine",
    .desc = "TIZEN base board(ARM)",
    .init = tizen_arm_board_init,
    .max_cpus = 255,
};

static void tizen_machine_init(void)
{
#if defined(TARGET_I386)
    qemu_register_machine(&tizen_x86_machine);
#elif defined(TARGET_ARM)
    qemu_register_machine(&tizen_arm_machine);
#else
#error
#endif
}

machine_init(tizen_machine_init);
