/*
 * Qemu webcam device emulation by PCI bus
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact:
 * JinHyung Jo <jinhyung.jo@samsung.com>
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


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include "qemu-common.h"
#include "cpu-common.h"

#include "pc.h"
#include "pci.h"
#include "pci_ids.h"

#include "svcamera.h"
#include "tizen/src/debug_ch.h"

MULTI_DEBUG_CHANNEL(tizen, camera_pci);

#define PCI_CAMERA_DEVICE_NAME      "svcamera_pci"

#define SVCAM_MEM_SIZE      (4 * 1024 * 1024)   // 4MB
#define SVCAM_REG_SIZE      (256)               // 64 * 4

/*
 *  I/O functions
 */
static inline uint32_t svcam_reg_read(void *opaque, target_phys_addr_t offset)
{
    uint32_t ret = 0;
    SVCamState *state = (SVCamState*)opaque;

    switch (offset & 0xFF) {
    case SVCAM_CMD_ISSTREAM:
        pthread_mutex_lock(&state->thread->mutex_lock);
        ret = state->streamon;
        pthread_mutex_unlock(&state->thread->mutex_lock);
        break;
    case SVCAM_CMD_G_DATA:
        ret = state->thread->param->stack[state->thread->param->top++];
        break;
    case SVCAM_CMD_OPEN:
    case SVCAM_CMD_CLOSE:
    case SVCAM_CMD_START_PREVIEW:
    case SVCAM_CMD_STOP_PREVIEW:
    case SVCAM_CMD_S_PARAM:
    case SVCAM_CMD_G_PARAM:
    case SVCAM_CMD_ENUM_FMT:
    case SVCAM_CMD_TRY_FMT:
    case SVCAM_CMD_S_FMT:
    case SVCAM_CMD_G_FMT:
    case SVCAM_CMD_QCTRL:
    case SVCAM_CMD_S_CTRL:
    case SVCAM_CMD_G_CTRL:
    case SVCAM_CMD_ENUM_FSIZES:
    case SVCAM_CMD_ENUM_FINTV:
        ret = state->thread->param->errCode;
        state->thread->param->errCode = 0;
        break;
    default:
        WARN("Not supported command!!\n");
        break;
    }
    return ret;
}

static inline void svcam_reg_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
    SVCamState *state = (SVCamState*)opaque;
    
    switch(offset & 0xFF) {
    case SVCAM_CMD_OPEN:
        svcam_device_open(state);
        break;
    case SVCAM_CMD_CLOSE:
        svcam_device_close(state);
        break;
    case SVCAM_CMD_START_PREVIEW:
        svcam_device_start_preview(state);
        break;
    case SVCAM_CMD_STOP_PREVIEW:
        svcam_device_stop_preview(state);
        break;
    case SVCAM_CMD_S_PARAM:
        svcam_device_s_param(state);
        break;
    case SVCAM_CMD_G_PARAM:
        svcam_device_g_param(state);
        break;
    case SVCAM_CMD_ENUM_FMT:
        svcam_device_enum_fmt(state);
        break;
    case SVCAM_CMD_TRY_FMT:
        svcam_device_try_fmt(state);
        break;
    case SVCAM_CMD_S_FMT:
        svcam_device_s_fmt(state);
        break;
    case SVCAM_CMD_G_FMT:
        svcam_device_g_fmt(state);
        break;
    case SVCAM_CMD_QCTRL:
        svcam_device_qctrl(state);
        break;
    case SVCAM_CMD_S_CTRL:
        svcam_device_s_ctrl(state);
        break;
    case SVCAM_CMD_G_CTRL:
        svcam_device_g_ctrl(state);
        break;
    case SVCAM_CMD_ENUM_FSIZES:
        svcam_device_enum_fsizes(state);
        break;
    case SVCAM_CMD_ENUM_FINTV:
        svcam_device_enum_fintv(state);
        break;
    case SVCAM_CMD_S_DATA:
        state->thread->param->stack[state->thread->param->top++] = value;
        break;
    case SVCAM_CMD_DATACLR:
        memset(state->thread->param, 0, sizeof(SVCamParam));
        break;
    case SVCAM_CMD_CLRIRQ:
        qemu_irq_lower(state->dev.irq[2]);
        break;
    case SVCAM_CMD_REQFRAME:
        pthread_mutex_lock(&state->thread->mutex_lock);
        state->req_frame = value + 1;
        pthread_mutex_unlock(&state->thread->mutex_lock);
        break;
    default:
        WARN("Not supported command!!\n");
        break;
    }
}

static CPUReadMemoryFunc * const svcam_reg_readfn[3] = {
    svcam_reg_read,
    svcam_reg_read,
    svcam_reg_read,
};

static CPUWriteMemoryFunc * const svcam_reg_writefn[3] = {
    svcam_reg_write,
    svcam_reg_write,
    svcam_reg_write,
};

/*
 *  memory allocation
 */
static void svcam_memory_map(PCIDevice *dev, int region_num,
                   pcibus_t addr, pcibus_t size, int type)
{
    SVCamState *s = DO_UPCAST(SVCamState, dev, dev);

    cpu_register_physical_memory(addr, size, s->mem_offset);
    s->mem_addr = addr;
}

static void svcam_mmio_map(PCIDevice *dev, int region_num,
                   pcibus_t addr, pcibus_t size, int type)
{
    SVCamState *s = DO_UPCAST(SVCamState, dev, dev);

    cpu_register_physical_memory(addr, size, s->cam_mmio);
    s->mmio_addr = addr;
}

/*
 *  Initialize function
 */
static int svcam_initfn(PCIDevice *dev)
{
    SVCamState *s = DO_UPCAST(SVCamState, dev, dev);
    uint8_t *pci_conf = s->dev.config;
    SVCamThreadInfo *thread;
    SVCamParam *param;

    pci_config_set_vendor_id(pci_conf, PCI_VENDOR_ID_TIZEN);
    pci_config_set_device_id(pci_conf, PCI_DEVICE_ID_VIRTUAL_CAMERA);
    pci_config_set_class(pci_conf, PCI_CLASS_MULTIMEDIA_OTHER);
    pci_config_set_interrupt_pin(pci_conf, 0x03);

    s->mem_offset = qemu_ram_alloc(NULL, "svcamera.ram", SVCAM_MEM_SIZE);
    /* return a host pointer */
    s->vaddr = qemu_get_ram_ptr(s->mem_offset);

    s->cam_mmio = cpu_register_io_memory(svcam_reg_readfn, svcam_reg_writefn,
                                            s, DEVICE_LITTLE_ENDIAN);

    /* setup memory space */
    /* memory #0 device memory (webcam buffer) */
    /* memory #1 memory-mapped I/O */
    pci_register_bar(&s->dev, 0, SVCAM_MEM_SIZE,
                        PCI_BASE_ADDRESS_MEM_PREFETCH, svcam_memory_map);

    pci_register_bar(&s->dev, 1, SVCAM_REG_SIZE,
                        PCI_BASE_ADDRESS_SPACE_MEMORY, svcam_mmio_map);

    /* for worker thread */
    thread = qemu_mallocz(sizeof(SVCamThreadInfo));
    param = qemu_mallocz(sizeof(SVCamParam));

    thread->state = s;
    thread->param = param;
    s->thread = thread;

    svcam_device_init(s);

    return 0;
}

int svcamera_pci_init(PCIBus *bus)
{
    pci_create_simple(bus, -1, PCI_CAMERA_DEVICE_NAME);
    return 0;
}

static PCIDeviceInfo svcamera_info = {
    .qdev.name    = PCI_CAMERA_DEVICE_NAME,
    .qdev.size    = sizeof(SVCamState),
    .no_hotplug   = 1,
    .init         = svcam_initfn,
};

static void svcamera_pci_register(void)
{
    pci_qdev_register(&svcamera_info);
}

device_init(svcamera_pci_register);
