/*
 * EFM32GG Timer
 *
 * Copyright (C) 2014 Rabin Vincent <rabin@rab.in>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "hw/sysbus.h"
#include "sysemu/sysemu.h"
#include "qemu/timer.h"

enum {
    TIMER_CTRL              = 0x00,
    TIMER_CMD               = 0x04,
    TIMER_IEN               = 0x0C,
    TIMER_IF                = 0x10,
    TIMER_IFS               = 0x14,
    TIMER_IFC               = 0x18,
    TIMER_TOP               = 0x1c,
    TIMER_CNT               = 0x24,
    TIMER_REG_SIZE          = TIMER_CNT + 4,

    TIMER_CTRL_OSMEN        = 0x10,
    TIMER_CTRL_MODE_UP      = 0x0,
    TIMER_CTRL_MODE_DOWN    = 0x1,

    TIMER_CMD_START         = 1 << 0,
    TIMER_CMD_STOP          = 1 << 1,

    TIMER_IF_OF             = 1 << 0,
    TIMER_IF_UF             = 1 << 1,
};

#define REG(s, x) (s->regs[(x) / 4])

#define TYPE_EFM32_TIMER "efm32-timer"
#define EFM32_TIMER(obj) \
    OBJECT_CHECK(Efm32TimerState, (obj), TYPE_EFM32_TIMER)

typedef struct Efm32TimerState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t rate;
    QEMUTimer *timer;
    uint32_t regs[TIMER_REG_SIZE / 4];
    qemu_irq irq;
} Efm32TimerState;

#define HFPERCLK_RATE 48000000

static uint64_t timer_to_ns(Efm32TimerState *s, uint64_t value)
{
    return muldiv64(value, get_ticks_per_sec(), s->rate);
}

static uint64_t ns_to_timer(Efm32TimerState *s, uint64_t value)
{
    return muldiv64(value, s->rate, get_ticks_per_sec());
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
        VMSTATE_TIMER(timer, Efm32TimerState),
        VMSTATE_UINT32_ARRAY(regs, Efm32TimerState, TIMER_REG_SIZE / 4),
        VMSTATE_END_OF_LIST()
    }
};

static int efm32_timer_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    Efm32TimerState *s = EFM32_TIMER(dev);

    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->iomem, OBJECT(s), &timer_ops, s,
                          "timer", 0x400);
    sysbus_init_mmio(sbd, &s->iomem);

    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, timer_tick, s);
    vmstate_register(dev, -1, &vmstate_efm32_timer, s);
    return 0;
}

static void efm32_timer_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = efm32_timer_init;
}

static const TypeInfo efm32_timer_info = {
    .name          = TYPE_EFM32_TIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Efm32TimerState),
    .class_init    = efm32_timer_class_init,
};

static void efm32_register_types(void)
{
    type_register_static(&efm32_timer_info);
}

type_init(efm32_register_types)
