/*
 * Emulator
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

#include "emulator.h"
#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, arch_arm);

extern int sensor_update(uint16_t x, uint16_t y, uint16_t z);

// fake sensor_update
int sensor_update(uint16_t x, uint16_t y, uint16_t z)
{
    TRACE( "x(%d), y(%d), z(%d) \n", x, y, z);
    return 0;
}

int qemu_arch_is_arm(void)
{
    return 1;
}

/* arm */
int device_set_rotation(int rotation)
{
    switch (rotation) {
        case 0:
            sensor_update(1, -256, 1);
            break;
        case 90:
            sensor_update(256, 1, 1);
            break;
        case 180:
            sensor_update(1, 256, 1);
            break;
        case 270:
            sensor_update(-256, 1, 1);
            break;
        default:
            assert(0);
    }
    return 0;
}

