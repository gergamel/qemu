/*
 * EFM32GG UART
 *
 * Copyright (c) 2014 Rabin Vincent <rabin@rab.in>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "hw/hw.h"
#include "hw/sysbus.h"
#include "sysemu/char.h"

enum {
    UARTn_CMD               = 0x0c,
    UARTn_STATUS            = 0x10,
    UARTn_RXDATAX           = 0x18,
    UARTn_TXDATA            = 0x34,
    UARTn_IF                = 0x40,
    UARTn_IFS               = 0x44,
    UARTn_IFC               = 0x48,
    UARTn_IEN               = 0x4c,
    UARTn_REG_SIZE          = UARTn_IEN + 4,

    UARTn_STATUS_TXBL       = 1 << 6,
    UARTn_STATUS_RXFULL     = 1 << 8,
    UARTn_STATUS_RXDATAV    = 1 << 7,

    UARTn_RXDATAX_FERR      = 1 << 15,

    UARTn_IF_RXDATAV        = 1 << 2,
};

#define REG(s, x) (s->regs[(x) / 4])

#define TYPE_EFM32_UART "efm32-uart"
#define EFM32_UART(obj) \
    OBJECT_CHECK(Efm32UartState, (obj), TYPE_EFM32_UART)

struct Efm32UartState {
    SysBusDevice parent_obj;

    MemoryRegion regs_region;
    CharDriverState *chr;
    qemu_irq rxirq;
    qemu_irq txirq;

    uint32_t regs[UARTn_REG_SIZE / 4];
};

typedef struct Efm32UartState Efm32UartState;

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

static uint64_t efm32_uart_read(void *opaque, hwaddr addr, unsigned size)
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
        if (s->chr) {
            qemu_chr_fe_write(s->chr, &ch, 1);
        }
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
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void efm32_uart_rx(void *opaque, const uint8_t *buf, int size)
{
    Efm32UartState *s = opaque;

    REG(s, UARTn_STATUS) |= UARTn_STATUS_RXDATAV;
    REG(s, UARTn_RXDATAX) = *buf;

    efm32_uart_update_irq(s);
}

static int efm32_uart_can_rx(void *opaque)
{
    Efm32UartState *s = opaque;

    return !(REG(s, UARTn_STATUS) & UARTn_STATUS_RXDATAV);
}

static void efm32_uart_event(void *opaque, int event)
{
    Efm32UartState *s = opaque;

    if (event == CHR_EVENT_BREAK) {
        REG(s, UARTn_STATUS) |= UARTn_STATUS_RXDATAV;
        REG(s, UARTn_RXDATAX) = UARTn_RXDATAX_FERR;
        efm32_uart_update_irq(s);
    }
}

static void efm32_uart_reset(DeviceState *d)
{
    Efm32UartState *s = EFM32_UART(d);

    memset(&s->regs, 0, sizeof(s->regs));
    REG(s, UARTn_STATUS) |= UARTn_STATUS_TXBL;
}

static void efm32_uart_realize(DeviceState *dev, Error **errp)
{
    Efm32UartState *s = EFM32_UART(dev);

    s->chr = qemu_char_get_next_serial();
    if (s->chr) {
        qemu_chr_add_handlers(s->chr, efm32_uart_can_rx, efm32_uart_rx,
                              efm32_uart_event, s);
    }
}

static void efm32_uart_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    Efm32UartState *s = EFM32_UART(obj);

    sysbus_init_irq(sbd, &s->rxirq);
    sysbus_init_irq(sbd, &s->txirq);

    memory_region_init_io(&s->regs_region, OBJECT(s), &uart_mmio_ops, s,
                          "efm32-uart", UARTn_REG_SIZE);
    sysbus_init_mmio(sbd, &s->regs_region);
}

static const VMStateDescription vmstate_efm32_uart = {
    .name = "efm32-uart",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, Efm32UartState, UARTn_REG_SIZE / 4),
        VMSTATE_END_OF_LIST()
    }
};

static void efm32_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = efm32_uart_realize;
    dc->reset = efm32_uart_reset;
    dc->vmsd = &vmstate_efm32_uart;
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
