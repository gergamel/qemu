/*
 * EFM32GG UART
 *
 * Copyright (c) 2014 Rabin Vincent <rabin@rab.in>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/char/efm32_uart.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

#ifndef EFM32_UART_ERR_DEBUG
#define EFM32_UART_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (EFM32_UART_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static void efm32_uart_update_irq(Efm32UartState *s)
{
    if (REG(s, UARTn_STATUS) & UARTn_STATUS_RXDATAV) {
        REG(s, UARTn_IF) |= UARTn_IF_RXDATAV;
    } else {
        REG(s, UARTn_IF) &= ~UARTn_IF_RXDATAV;
    }

    qemu_set_irq(s->rxirq, REG(s, UARTn_IF) & \
                           REG(s, UARTn_IEN) & \
                           UARTn_IF_RXDATAV);
}

static int efm32_uart_can_rx(void *opaque)
{
    Efm32UartState *s = opaque;

    return !(REG(s, UARTn_STATUS) & UARTn_STATUS_RXDATAV);
}

static void efm32_uart_rx(void *opaque, const uint8_t *buf, int size)
{
    Efm32UartState *s = opaque;

    REG(s, UARTn_STATUS) |= UARTn_STATUS_RXDATAV;
    REG(s, UARTn_RXDATAX) = *buf;

    efm32_uart_update_irq(s);
}

static void efm32_uart_reset(DeviceState *d)
{
    Efm32UartState *s = EFM32_UART(d);

    memset(&s->regs, 0, sizeof(s->regs));
    REG(s, UARTn_STATUS) |= UARTn_STATUS_TXBL;
}

static uint64_t efm32_uart_read(void *opaque, hwaddr addr, unsigned int size)
{
    Efm32UartState *s = opaque;
    uint32_t r = 0;

    switch (addr) {
    case UARTn_STATUS:
        return REG(s, UARTn_STATUS);
    case UARTn_RXDATAX:
        REG(s, UARTn_STATUS) &= ~UARTn_STATUS_RXDATAV;
        return REG(s, UARTn_RXDATAX);
    case UARTn_IF:
        return REG(s, UARTn_IF);
    case UARTn_IEN:
        return REG(s, UARTn_IEN);
    }

    efm32_uart_update_irq(s);

    return r;
}

static void efm32_uart_write(void *opaque, hwaddr addr,
                             uint64_t value, unsigned size)
{
    Efm32UartState *s = opaque;
    unsigned char ch = value;

    switch (addr) {
    case UARTn_TXDATA:
        qemu_chr_fe_write(&s->chr, &ch, 1);
        break;
    case UARTn_IFS:
        REG(s, UARTn_IF) |= value;
        break;
    case UARTn_IFC:
        REG(s, UARTn_IF) &= ~value;
        break;
    case UARTn_IEN:
        REG(s, UARTn_IEN) = value;
        break;
    }

    efm32_uart_update_irq(s);
}

static const MemoryRegionOps uart_mmio_ops = {
    .read = efm32_uart_read,
    .write = efm32_uart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property efm32_uart_properties[] = {
    DEFINE_PROP_CHR("chardev", Efm32UartState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void efm32_uart_event(void *opaque, QEMUChrEvent event)
{
    Efm32UartState *s = opaque;

    if (event == CHR_EVENT_BREAK) {
        REG(s, UARTn_STATUS) |= UARTn_STATUS_RXDATAV;
        REG(s, UARTn_RXDATAX) = UARTn_RXDATAX_FERR;
        efm32_uart_update_irq(s);
    }
}

static void efm32_uart_realize(DeviceState *dev, Error **errp)
{
    Efm32UartState *s = EFM32_UART(dev);

    /*
    s->chr = qemu_char_get_next_serial();
    if (s->chr) {
        qemu_chr_add_handlers(s->chr, efm32_uart_can_rx, efm32_uart_rx,
                              efm32_uart_event, s);
    }*/
    qemu_chr_fe_set_handlers(&s->chr, efm32_uart_can_rx,
                             efm32_uart_rx, efm32_uart_event, NULL,
                             s, NULL, true);
}

static void efm32_uart_init(Object *obj)
{
    Efm32UartState *s = EFM32_UART(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->rxirq);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->txirq);

    memory_region_init_io(&s->regs_region, OBJECT(s), &uart_mmio_ops, s,
                          TYPE_EFM32_UART, UARTn_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->regs_region);
}

/*
static const VMStateDescription vmstate_efm32_uart = {
    .name = "efm32-uart",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, Efm32UartState, UARTn_REG_SIZE / 4),
        VMSTATE_END_OF_LIST()
    }
};*/

static void efm32_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = efm32_uart_reset;
    device_class_set_props(dc, efm32_uart_properties);
    dc->realize = efm32_uart_realize;
    //dc->vmsd = &vmstate_efm32_uart;
}

static const TypeInfo efm32_uart_info = {
    .name          = TYPE_EFM32_UART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Efm32UartState),
    .instance_init = efm32_uart_init,
    .class_init    = efm32_uart_class_init,
};

static void efm32_uart_register_types(void)
{
    type_register_static(&efm32_uart_info);
}

type_init(efm32_uart_register_types)
