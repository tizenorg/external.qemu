/*
 * Maru Virtual USB Touchscreen emulation.
 * Based on hw/usb-wacom.c:
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact:
 *  GiWoong Kim <giwoong.kim@samsung.com>
 *  Hyunjun Son <hj79.son@samsung.com>
 *  DongKyun Yun <dk77.yun@samsung.com>
 *  YeongKyoon Lee <yeongkyoon.lee@samsung.com>
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

#include "hw.h"
#include "console.h"
#include "usb.h"
#include "usb-desc.h"


typedef struct USBTouchscreenState {
    USBDevice dev;
    QEMUPutMouseEntry *eh_entry;

    int32_t dx, dy, dz, buttons_state;
    int8_t mouse_grabbed;
    int8_t changed;
} USBTouchscreenState;

/* This structure must match the kernel definitions */
typedef struct USBEmulTouchscreenPacket {
    uint16_t x, y, z;
    uint8_t state;
} USBEmulTouchscreenPacket;


#define EMUL_TOUCHSCREEN_PACKET_LEN 7
#define TOUCHSCREEN_RESOLUTION_X 5040
#define TOUCHSCREEN_RESOLUTION_Y 3780

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER]     = "QEMU " QEMU_VERSION,
    [STR_PRODUCT]          = "Maru Virtual Touchscreen",
    [STR_SERIALNUMBER]     = "1",
};

static const USBDescIface desc_iface_touchscreen = {
    .bInterfaceNumber              = 0,
    .bNumEndpoints                 = 1,
    .bInterfaceClass               = USB_CLASS_HID,
    .bInterfaceSubClass            = 0x01, /* boot */
    .bInterfaceProtocol            = 0x02,
    .ndesc                         = 1,
    .descs = (USBDescOther[]) {
        {
            /* HID descriptor */
            .data = (uint8_t[]) {
                0x09,          /*  u8  bLength */
                0x21,          /*  u8  bDescriptorType */
                0x01, 0x10,    /*  u16 HID_class */
                0x00,          /*  u8  country_code */
                0x01,          /*  u8  num_descriptors */
                0x22,          /*  u8  type: Report */
                0x6e, 0,       /*  u16 len */
            },
        },
    },
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress      = USB_DIR_IN | 0x01,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 8,
            .bInterval             = 0x0a,
        },
    },
};

static const USBDescDevice desc_device_touchscreen = {
    .bcdUSB                        = 0x0110,
    .bMaxPacketSize0               = EMUL_TOUCHSCREEN_PACKET_LEN + 1,
    .bNumConfigurations            = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces        = 1,
            .bConfigurationValue   = 1,
            .bmAttributes          = 0x80,
            .bMaxPower             = 40,
            .ifs = &desc_iface_touchscreen,
        },
    },
};

static const USBDesc desc_touchscreen = {
    .id = {
        .idVendor          = 0x056a,
        .idProduct         = 0x0000,
        .bcdDevice         = 0x0010,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_touchscreen,
    .str = desc_strings,
};

static void usb_touchscreen_event(void *opaque, int x, int y, int z, int buttons_state)
{
    USBTouchscreenState *s = opaque;

    /* scale to resolution */
    s->dx = (x * TOUCHSCREEN_RESOLUTION_X / 0x7FFF);
    s->dy = (y * TOUCHSCREEN_RESOLUTION_Y / 0x7FFF);
    s->dz = z;
    s->buttons_state = buttons_state;
    s->changed = 1;
}

static int usb_touchscreen_poll(USBTouchscreenState *s, uint8_t *buf, int len)
{
    USBEmulTouchscreenPacket *packet = (USBEmulTouchscreenPacket *)buf;

    if (s->mouse_grabbed == 0) {
        s->eh_entry = qemu_add_mouse_event_handler(usb_touchscreen_event, s, 1, "QEMU Virtual touchscreen");
        qemu_activate_mouse_event_handler(s->eh_entry);
        s->mouse_grabbed = 1;
    }

    if (len < EMUL_TOUCHSCREEN_PACKET_LEN) {
        return 0;
    }

    packet->x = s->dx & 0xffff;
    packet->y = s->dy & 0xffff;
    packet->z = s->dz & 0xffff;

    if (s->buttons_state == 0) {
        packet->state = 0;
    } else {
        if (s->buttons_state & MOUSE_EVENT_LBUTTON) {
            packet->state |= 1;
        }
        if (s->buttons_state & MOUSE_EVENT_RBUTTON) {
            packet->state |= 2;
        }
     if (s->buttons_state & MOUSE_EVENT_MBUTTON) {
            packet->state |= 4;
          }
    }

    return EMUL_TOUCHSCREEN_PACKET_LEN;
}

static void usb_touchscreen_handle_reset(USBDevice *dev)
{
    USBTouchscreenState *s = (USBTouchscreenState *) dev;

    s->dx = 0;
    s->dy = 0;
    s->dz = 0;
    s->buttons_state = 0;
}

static int usb_touchscreen_handle_control(USBDevice *dev,
    int request, int value, int index, int length, uint8_t *data)
{
    return usb_desc_handle_control(dev, request, value, index, length, data);
}

static int usb_touchscreen_handle_data(USBDevice *dev, USBPacket *p)
{
    USBTouchscreenState *s = (USBTouchscreenState *) dev;
    int ret = 0;

    switch (p->pid) {
    case USB_TOKEN_IN:
        if (p->devep == 1) {
            if (s->changed == 0) {
                return USB_RET_NAK;
            }

            s->changed = 0;
            ret = usb_touchscreen_poll(s, p->data, p->len);
            break;
        }
        /* Fall through */
    case USB_TOKEN_OUT:
    default:
        ret = USB_RET_STALL;
        break;
    }
    return ret;
}

static void usb_touchscreen_handle_destroy(USBDevice *dev)
{
    USBTouchscreenState *s = (USBTouchscreenState *) dev;

    if (s->mouse_grabbed == 1) {
        qemu_remove_mouse_event_handler(s->eh_entry);
        s->mouse_grabbed = 0;
    }
}

static int usb_touchscreen_initfn(USBDevice *dev)
{
    USBTouchscreenState *s = DO_UPCAST(USBTouchscreenState, dev, dev);
    usb_desc_init(dev);
    s->changed = 1;
    return 0;
}

/* Remove mouse handlers before loading.  */
static int touchscreen_pre_load(void *opaque)
{
    USBTouchscreenState *s = (USBTouchscreenState *)opaque;

    if (s->eh_entry) {
        qemu_remove_mouse_event_handler(s->eh_entry);
    }

    return 0;
}

static int touchscreen_post_load(void *opaque, int version_id)
{
    USBTouchscreenState *s = (USBTouchscreenState *)opaque;

    s->changed = 1;
    if (s->mouse_grabbed == 1) {
        s->eh_entry = qemu_add_mouse_event_handler(usb_touchscreen_event, s, 1, "QEMU Virtual touchscreen");
        qemu_activate_mouse_event_handler(s->eh_entry);
    }

    return 0;
}

static VMStateDescription vmsd_usbdevice = {
    .name = "maru-touchscreen-usbdevice",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField []) {
        VMSTATE_UINT8(addr, USBDevice),
        VMSTATE_INT32(state, USBDevice),
        VMSTATE_END_OF_LIST()
    }
};

static VMStateDescription vmsd = {
    .name = "maru-touchscreen",
    .version_id = 2,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .pre_load = touchscreen_pre_load,
    .post_load = touchscreen_post_load,
    .fields = (VMStateField []) {
        VMSTATE_STRUCT(dev, USBTouchscreenState, 1, vmsd_usbdevice, USBDevice),
        VMSTATE_INT32(dx, USBTouchscreenState),
        VMSTATE_INT32(dy, USBTouchscreenState),
        VMSTATE_INT32(dz, USBTouchscreenState),
        VMSTATE_INT32(buttons_state, USBTouchscreenState),
        VMSTATE_INT8(mouse_grabbed, USBTouchscreenState),
        VMSTATE_INT8(changed, USBTouchscreenState),
        VMSTATE_END_OF_LIST()
    }
};

static struct USBDeviceInfo touchscreen_info = {
    .product_desc   = "QEMU Virtual Touchscreen",
    .qdev.name      = "usb-maru-touchscreen",
    .qdev.desc      = "QEMU Virtual Touchscreen",
    .usbdevice_name = "maru-touchscreen",
    .usb_desc       = &desc_touchscreen,
    .qdev.size      = sizeof(USBTouchscreenState),
    .qdev.vmsd      = &vmsd,
    .init           = usb_touchscreen_initfn,
    .handle_packet  = usb_generic_handle_packet,
    .handle_reset   = usb_touchscreen_handle_reset,
    .handle_control = usb_touchscreen_handle_control,
    .handle_data    = usb_touchscreen_handle_data,
    .handle_destroy = usb_touchscreen_handle_destroy,
};

static void usb_touchscreen_register_devices(void)
{
    usb_qdev_register(&touchscreen_info);
}

device_init(usb_touchscreen_register_devices)
