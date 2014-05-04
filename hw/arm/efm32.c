/*
 * EFM32GG
 *
 * Copyright (C) 2014 Rabin Vincent <rabin@rab.in>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "hw/devices.h"
#include "hw/boards.h"
#include "hw/block/flash.h"
#include "exec/address-spaces.h"

static void efm32_init(QEMUMachineInitArgs *args)
{
    static const int timer_irq[] = {2, 12, 13, 14};
    const char *cpu_model = args->cpu_model;
    const char *kernel_filename = args->kernel_filename;
    MemoryRegion *address_space_mem = get_system_memory();
    qemu_irq *pic;
    int i;
    MemoryRegion *nor = g_new(MemoryRegion, 1);
    MemoryRegion *psram = g_new(MemoryRegion, 1);
    DriveInfo *dinfo;
    uint32_t flashsize = 16 * 1024 * 1024;

    pic = armv7m_init(address_space_mem, 1024, 128, kernel_filename, cpu_model);

    memory_region_init_ram(psram, NULL, "psram", 4 * 1024 * 1024);
    vmstate_register_ram_global(psram);
    memory_region_add_subregion(address_space_mem, 0x88000000, psram);

    memory_region_init_ram(nor, NULL, "flash", flashsize);
    vmstate_register_ram_global(nor);
    memory_region_add_subregion(address_space_mem, 0x8C000000, nor);
    memory_region_set_readonly(nor, true);

    sysbus_create_varargs("efm32-uart", 0x4000e400, pic[22], pic[23], NULL);
    for (i = 0; i < 4; i++) {
        sysbus_create_simple("efm32-timer", 0x40010000 + i * 0x400,
                             pic[timer_irq[i]]);
    }

    dinfo = drive_get(IF_PFLASH, 0, 0);
    if (dinfo) {
        int sector_len = 128 * 1024;
        if (!pflash_cfi01_register(0x8c000000, NULL, "dk.flash", flashsize,
                                   dinfo ? dinfo->bdrv : NULL,
                                   sector_len, flashsize / sector_len,
                                   2, 0, 0, 0, 0, 0)) {
            fprintf(stderr, "qemu: Error registering flash memory.\n");
            exit(1);
        }
    }
}

static QEMUMachine efm32ggdk3750_machine = {
    .name = "efm32ggdk3750",
    .desc = "EnergyMicro Giant Gecko Development Kit",
    .init = efm32_init,
};

static void efm32_machine_init(void)
{
    qemu_register_machine(&efm32ggdk3750_machine);
}

machine_init(efm32_machine_init);
