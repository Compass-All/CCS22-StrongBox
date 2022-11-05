#ifndef STRONGBOX_DEFS
#define STRONGBOX_DEFS
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lib/spinlock.h>
#include <drivers/console.h>

// TZASC
#define PLAT_ARM_TZC_BASE UL(0x2a4a0000)
#define ID_ACCESS 0x114U
#define REGION_REGISTER_OFFSET(register,region_num) ((register)+0x20U*(region_num))
#define TZASC_FOR_ALL 0
#define TZASC_FOR_SECURE_ONLY 1
#define TZASC_FOR_GPU_SECURE 2

// Mali GPU
#define GPU_MMIO_BASE (void*)0x2d000000
#define JOB_SLOT0 0x800
#define JS_COMMAND_START 0x01
#define JS_STATUS 0x24	/* (RO) Status register for job slot n */
#define JS_COMMAND_NEXT 0x60
#define JS_HEAD_NEXT_LO 0x40 /* (RW) Next job queue head pointer for job slot n, low word */
#define JS_HEAD_NEXT_HI 0x44 /* (RW) Next job queue head pointer for job slot n, high word */
#define JS_CONFIG_NEXT 0x58 /* (RW) Next configuration settings for job slot n */
#define JOB_CONTROL_BASE  0x1000
#define JOB_CONTROL_REG(r) (JOB_CONTROL_BASE + (r))
#define JOB_SLOT_REG(n, r) (JOB_CONTROL_REG(JOB_SLOT0 + ((n) << 7)) + (r))
#define MEMORY_MANAGEMENT_BASE  0x2000
#define MMU_REG(r)              (MEMORY_MANAGEMENT_BASE + (r))
#define MMU_AS0                 0x400	/* Configuration registers for address space 0 */
#define MMU_AS_REG(n, r)        (MMU_REG(MMU_AS0 + ((n) << 6)) + (r))
#define AS_TRANSTAB_LO         0x00	/* (RW) Translation Table Base Address for address space n, low word */
#define AS_TRANSTAB_HI         0x04	/* (RW) Translation Table Base Address for address space n, high word */

// GPU TEE
uint64_t jc_phys;
uint32_t as_nr;
uint64_t mmu_pgd;
uint64_t* operation_buffer_phys; // [virt_base(8 bytes) flag(8 bytes)] end with 0
uint64_t aes_key[2];
int katom_count;

struct gpu_buffer {
	void *virt_base; // do not use phys_base due to the buffer length may exceed 4K
	size_t size;
    int flag;
    int hash;
} gpu_buffer[8];
int buffer_count;
#endif
