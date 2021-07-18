/*
 * EFM32GG
 *
 * Copyright (C) 2014 Rabin Vincent <rabin@rab.in>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

//#include "hw/sysbus.h"
//#include "hw/arm/arm.h"
//#include "hw/sysbus.h"
//#include "qom/object.h"
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/arm/efm32.h"
#include "hw/qdev-properties.h"
#include "sysemu/sysemu.h"

static const int uart_tx_irq[] = {15,16};
//static const int uart_rx_irq[] = {17,18};
static const int timer_irq[] = {2, 12, 13, 14};

static void efm32_realize(DeviceState *dev_soc, Error **errp)
{
    Efm32State *s = EFM32(dev_soc);
    DeviceState *dev, *armv7m;
    SysBusDevice *busdev;
    int i;
    uint32_t flashsize = 16 * 1024 * 1024;
    uint32_t sramsize = 4 * 1024 * 1024;

    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);

    memory_region_init_rom(flash, OBJECT(dev_soc), "efm32.flash", flashsize, &error_fatal);

    memory_region_add_subregion(system_memory, 0x00000000, flash);
    
    memory_region_init_ram(sram, NULL, "efm32.sram", sramsize, &error_fatal);
    memory_region_add_subregion(system_memory, 0x20000000, sram);

    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 96);
    qdev_prop_set_string(armv7m, "cpu-type", s->cpu_type);
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(get_system_memory()), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }

    /* System configuration controller */
    dev = DEVICE(&s->syscfg);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->syscfg), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, 0x4000e400);

    /* Attach UART (uses USART registers) and USART controllers */
    for (i = 0; i < EFM32_NUM_UARTS; i++) {
        dev = DEVICE(&(s->uart[i]));
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->uart[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, 0x40010000 + i * 0x400);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, uart_tx_irq[i]));
        //sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, uart_rx_irq[i]));
    }

    /* Attach UART (uses USART registers) and USART controllers */
    for (i = 0; i < EFM32_NUM_TIMERS; i++) {
        dev = DEVICE(&(s->timer[i]));
        qdev_prop_set_uint64(dev, "clock-frequency", HFPERCLK_RATE);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->timer[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, 0x40020000 + i * 0x400);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, timer_irq[i]));
    }
}

static void efm32_initfn(Object *obj)
{
    Efm32State *s = EFM32(obj);
    int i;

    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);

    object_initialize_child(obj, "syscfg", &s->syscfg, TYPE_EFM32);

    for (i = 0; i < EFM32_NUM_UARTS; i++) {
        object_initialize_child(obj, "uart[*]", &s->uart[i], TYPE_EFM32_UART);
    }

    for (i = 0; i < EFM32_NUM_TIMERS; i++) {
        object_initialize_child(obj, "timer[*]", &s->timer[i], TYPE_EFM32_TIMER);
    }
}

static Property efm32_properties[] = {
    DEFINE_PROP_STRING("cpu-type", Efm32State, cpu_type),
    DEFINE_PROP_END_OF_LIST(),
};

static void efm32_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = efm32_realize;
    device_class_set_props(dc, efm32_properties);
}

static const TypeInfo efm32_info = {
    .name          = TYPE_EFM32,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Efm32State),
    .instance_init = efm32_initfn,
    .class_init    = efm32_class_init,
};

static void efm32_types(void)
{
    type_register_static(&efm32_info);
}

type_init(efm32_types)
