/*
 * BBC micro:bit machine
 * http://tech.microbit.org/hardware/
 *
 * Copyright 2018 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"

#include "hw/arm/efm32.h"
#include "hw/qdev-properties.h"
#include "qom/object.h"

struct STK3701AMachineState {
    MachineState parent;
    Efm32State efm32;
};

#define TYPE_STK3701A_MACHINE MACHINE_TYPE_NAME("STK3701A")

OBJECT_DECLARE_SIMPLE_TYPE(STK3701AMachineState, STK3701A_MACHINE)

static void STK3701A_init(MachineState *machine)
{
    STK3701AMachineState *s = STK3701A_MACHINE(machine);
    //MemoryRegion *system_memory = get_system_memory();
    //MemoryRegion *mr;

    object_initialize_child(OBJECT(machine), "efm32", &s->efm32, TYPE_EFM32);
    //object_property_set_link(OBJECT(&s->efm32), "memory", OBJECT(system_memory), &error_fatal);
    sysbus_realize(SYS_BUS_DEVICE(&s->efm32), &error_fatal);
    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename, 2*1024*1024);
}

static void STK3701A_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "EFM32GG Series 1 Starter Kit SLSTK3701A(Cortex-M4)";
    mc->init = STK3701A_init;
    mc->max_cpus = 1;
}

static const TypeInfo STK3701A_info = {
    .name = TYPE_STK3701A_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(STK3701AMachineState),
    .class_init = STK3701A_machine_class_init,
};

static void STK3701A_machine_init(void)
{
    type_register_static(&STK3701A_info);
}

type_init(STK3701A_machine_init);
