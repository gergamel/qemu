/*
 * STM32F2XX Timer
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

#ifndef HW_EFM32_TIMER_H
#define HW_EFM32_TIMER_H

#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "qom/object.h"

#define TIMER_CTRL              0x00
#define TIMER_CMD               0x04
#define TIMER_IEN               0x0C
#define TIMER_IF                0x10
#define TIMER_IFS               0x14
#define TIMER_IFC               0x18
#define TIMER_TOP               0x1c
#define TIMER_CNT               0x24
#define TIMER_REG_SIZE          TIMER_CNT + 4

#define TIMER_CTRL_OSMEN        0x10
#define TIMER_CTRL_MODE_UP      0x0
#define TIMER_CTRL_MODE_DOWN    0x1

#define TIMER_CMD_START         1 << 0
#define TIMER_CMD_STOP          1 << 1
#define TIMER_IF_OF             1 << 0
#define TIMER_IF_UF             1 << 1

#define HFPERCLK_RATE 48000000

#define REG(s, x) (s->regs[(x) / 4])

#define TYPE_EFM32_TIMER "efm32-timer"
OBJECT_DECLARE_SIMPLE_TYPE(Efm32TimerState, EFM32_TIMER)

struct Efm32TimerState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    QEMUTimer *timer;
    qemu_irq irq;

    uint32_t rate;
    uint32_t regs[TIMER_REG_SIZE / 4];
};

#endif /* HW_EFM32_TIMER_H */
