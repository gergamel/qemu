/*
 * EFM32GG Timer
 *
 * Copyright (C) 2014 Rabin Vincent <rabin@rab.in>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/timer/efm32_timer.h"

static uint64_t timer_to_ns(Efm32TimerState *s, uint64_t value)
{
    return muldiv64(value, NANOSECONDS_PER_SECOND, s->rate);
}

static uint64_t ns_to_timer(Efm32TimerState *s, uint64_t value)
{
    return muldiv64(value, s->rate, NANOSECONDS_PER_SECOND);
}

static uint32_t timer_mode(Efm32TimerState *s)
{
    return REG(s, TIMER_CTRL) & 0x3;
}

static void timer_update_irq(Efm32TimerState *s)
{
    qemu_set_irq(s->irq, REG(s, TIMER_IF) & REG(s, TIMER_IEN));
}

static void timer_tick(void *opaque)
{
    Efm32TimerState *s = opaque;
    uint32_t flag;

    if (timer_mode(s) == TIMER_CTRL_MODE_UP) {
        flag = TIMER_IF_OF;
    } else {
        flag = TIMER_IF_UF;
    }

    REG(s, TIMER_IF) |= flag;

    timer_update_irq(s);
}

static void efm32_timer_reset(DeviceState *dev)
{
    Efm32TimerState *s = EFM32_TIMER(dev);
    //int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    memset(s->regs,0,TIMER_REG_SIZE);
    //s->tick_offset = stm32f2xx_ns_to_ticks(s, now);
}

static uint64_t timer_read(void *opaque, hwaddr offset,
                          unsigned size)
{
    Efm32TimerState *s = opaque;

    switch (offset) {
    case TIMER_CTRL:
        return REG(s, TIMER_CTRL);
    case TIMER_IEN:
        return REG(s, TIMER_IEN);
    case TIMER_IF:
        return REG(s, TIMER_IF);
    case TIMER_CNT:
        return ns_to_timer(s, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL));
    }

    return 0;
}

static void timer_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size)
{
    Efm32TimerState *s = opaque;

    switch (offset) {
    case TIMER_CTRL:
        REG(s, TIMER_CTRL) = value;
        s->rate = HFPERCLK_RATE / (1 << ((value >> 24) & 0xff));
        break;
    case TIMER_IEN:
        REG(s, TIMER_IEN) = value;
        break;
    case TIMER_IFS:
        REG(s, TIMER_IF) |= value;
        break;
    case TIMER_IFC:
        REG(s, TIMER_IF) &= ~value;
        if (!(REG(s, TIMER_CTRL) & TIMER_CTRL_OSMEN)) {
            timer_mod(s->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
                                + timer_to_ns(s, REG(s, TIMER_TOP)));
        }
        break;
    case TIMER_TOP:
        REG(s, TIMER_TOP) = value;
        break;
    case TIMER_CMD:
        if (value & TIMER_CMD_START) {
            uint32_t cnt;
            uint32_t expiry;

            if (REG(s, TIMER_CTRL) & TIMER_CTRL_OSMEN) {
                cnt = REG(s, TIMER_CNT);
            } else {
                cnt = REG(s, TIMER_TOP);
            }

            if (timer_mode(s) & TIMER_CTRL_MODE_UP) {
                expiry = 0xffffffff - cnt;
            } else if (timer_mode(s) & TIMER_CTRL_MODE_DOWN) {
                expiry = cnt;
            }

            timer_mod(s->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) \
                                + timer_to_ns(s, expiry));
        } else if (value & TIMER_CMD_STOP) {
            timer_del(s->timer);
        }
        break;
    case TIMER_CNT:
        REG(s, TIMER_CNT) = value;
        break;
    }

    timer_update_irq(s);
}

static const MemoryRegionOps timer_ops = {
    .read = timer_read,
    .write = timer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_efm32_timer = {
    .name = "efm32-timer",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(rate, Efm32TimerState),
        VMSTATE_UINT32_ARRAY(regs, Efm32TimerState, TIMER_REG_SIZE / 4),
        VMSTATE_END_OF_LIST()
    }
};

static void efm32_timer_init(Object *obj)
{
    Efm32TimerState *s = EFM32_TIMER(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->iomem, obj, &timer_ops, s,
                          "efm32_timer", 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
}


static void efm32_timer_realize(DeviceState *dev, Error **errp)
{
    Efm32TimerState *s = EFM32_TIMER(dev);
    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, timer_tick, s);
}

static void efm32_timer_class_init(ObjectClass *klass, void *data)
/*{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = efm32_timer_init;
}*/
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = efm32_timer_reset;
    //device_class_set_props(dc, stm32f2xx_timer_properties);
    dc->vmsd = &vmstate_efm32_timer;
    dc->realize = efm32_timer_realize;
}

static const TypeInfo efm32_timer_info = {
    .name          = TYPE_EFM32_TIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Efm32TimerState),
    .instance_init = efm32_timer_init,
    .class_init    = efm32_timer_class_init,
};

static void efm32_register_types(void)
{
    type_register_static(&efm32_timer_info);
}

type_init(efm32_register_types)
