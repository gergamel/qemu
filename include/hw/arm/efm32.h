/*
 * STM32F205 SoC
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

#ifndef HW_ARM_EFM32_H
#define HW_ARM_EFM32_H

#include "hw/misc/efm32_syscfg.h"
#include "hw/timer/efm32_timer.h"
#include "hw/char/efm32_uart.h"
#include "hw/or-irq.h"
#include "hw/arm/armv7m.h"
#include "qom/object.h"

#define TYPE_EFM32 "efm32"
OBJECT_DECLARE_SIMPLE_TYPE(Efm32State, EFM32)

#define EFM32_NUM_UARTS  1
#define EFM32_NUM_TIMERS 4

#define FLASH_BASE_ADDRESS 0x00000000
#define FLASH_SIZE (2 * 1024 * 1024)
#define SRAM_BASE_ADDRESS 0x20000000
#define SRAM_SIZE (128 * 1024)

struct Efm32State {
    SysBusDevice parent_obj;
    char *cpu_type;
    ARMv7MState armv7m;
    
    Efm32SyscfgState syscfg;
    Efm32UartState uart[EFM32_NUM_UARTS];
    Efm32TimerState timer[EFM32_NUM_TIMERS];

    qemu_or_irq *adc_irqs;
};

#endif /* HW_ARM_EFM32_H */
