/*
 * AK8973 Compass Emulation
 *
 * Copyright (c) 2010 Samsung Electronics.
 * Contributed by Junsik.Park <okdear.park@samsung.com>
 *                Dmitry Zhurikhin <zhur@ispras.ru>
 */

void ak8973_update(DeviceState *dev,
                   uint8_t x, uint8_t y, uint8_t z, uint8_t temp);
