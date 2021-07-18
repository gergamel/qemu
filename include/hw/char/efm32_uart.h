/*
 * STM32F2XX USART
 *
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef HW_EFM32_UART_H
#define HW_EFM32_UART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define UARTn_CMD               0x0c
#define UARTn_STATUS            0x10
#define UARTn_RXDATAX           0x18
#define UARTn_TXDATA            0x34
#define UARTn_IF                0x40
#define UARTn_IFS               0x44
#define UARTn_IFC               0x48
#define UARTn_IEN               0x4c
#define UARTn_REG_SIZE          UARTn_IEN + 4
#define UARTn_STATUS_TXBL       1 << 6
#define UARTn_STATUS_RXFULL     1 << 8
#define UARTn_STATUS_RXDATAV    1 << 7
#define UARTn_RXDATAX_FERR      1 << 15
#define UARTn_IF_RXDATAV        1 << 2

/*
 * NB: The reset value mentioned in "24.6.1 Status register" seems bogus.
 * Looking at "Table 98 USART register map and reset values", it seems it
 * should be 0xc0, and that's how real hardware behaves.
 */
#define USART_SR_RESET (USART_SR_TXE | USART_SR_TC)

#define USART_SR_TXE  (1 << 7)
#define USART_SR_TC   (1 << 6)
#define USART_SR_RXNE (1 << 5)

#define USART_CR1_UE  (1 << 13)
#define USART_CR1_RXNEIE  (1 << 5)
#define USART_CR1_TE  (1 << 3)
#define USART_CR1_RE  (1 << 2)

#define TYPE_EFM32_UART "efm32-uart"
OBJECT_DECLARE_SIMPLE_TYPE(Efm32UartState, EFM32_UART)

struct Efm32UartState {
    SysBusDevice parent_obj;

    MemoryRegion regs_region;

    uint32_t regs[UARTn_REG_SIZE / 4];

    CharBackend chr;
    qemu_irq rxirq;
    qemu_irq txirq;
};

#define REG(s, x) (s->regs[(x) / 4])

#endif /* HW_EFM32_UART_H */
