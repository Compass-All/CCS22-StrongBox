/*
 * Copyright (c) 2014-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <lib/el3_runtime/cpu_data.h>
#include <lib/pmf/pmf.h>
#include <lib/psci/psci.h>
#include <lib/runtime_instr.h>
#include <services/sdei.h>
#include <services/spm_svc.h>
#include <services/std_svc.h>
#include <smccc_helpers.h>
#include <tools_share/uuid.h>

#include <drivers/arm/tzc400.h>
#include <plat/arm/common/arm_def.h>
#include <plat/arm/common/plat_arm.h>
#include <drivers/arm/tzc_common.h>

#include <bl31/access.h>
#include <bl31/gpu_task.h>
#include <bl31/memory_util.h>
#include <bl31/crypt.h>
#include <bl31/strongbox_defs.h>

#include <lib/mmio.h>
#include <arch_helpers.h>

/* Standard Service UUID */
static uuid_t arm_svc_uid = {
	{0x5b, 0x90, 0x8d, 0x10},
	{0x63, 0xf8},
	{0xe8, 0x47},
	0xae, 0x2d,
	{0xc0, 0xfb, 0x56, 0x41, 0xf6, 0xe2}
};

/* Setup Standard Services */
static int32_t std_svc_setup(void)
{
	uintptr_t svc_arg;
	int ret = 0;

	svc_arg = get_arm_std_svc_args(PSCI_FID_MASK);
	assert(svc_arg);

	/*
	 * PSCI is one of the specifications implemented as a Standard Service.
	 * The `psci_setup()` also does EL3 architectural setup.
	 */
	if (psci_setup((const psci_lib_args_t *)svc_arg) != PSCI_E_SUCCESS) {
		ret = 1;
	}

#if ENABLE_SPM
	if (spm_setup() != 0) {
		ret = 1;
	}
#endif

#if SDEI_SUPPORT
	/* SDEI initialisation */
	sdei_init();
#endif

	return ret;
}

/*
 * Top-level Standard Service SMC handler. This handler will in turn dispatch
 * calls to PSCI SMC handler
 */
static uintptr_t std_svc_smc_handler(uint32_t smc_fid,
			     u_register_t x1,
			     u_register_t x2,
			     u_register_t x3,
			     u_register_t x4,
			     void *cookie,
			     void *handle,
			     u_register_t flags)
{
	/*
	 * Dispatch PSCI calls to PSCI SMC handler and return its return
	 * value
	 */
	if (is_psci_fid(smc_fid)) {
		uint64_t ret;

#if ENABLE_RUNTIME_INSTRUMENTATION

		/*
		 * Flush cache line so that even if CPU power down happens
		 * the timestamp update is reflected in memory.
		 */
		PMF_WRITE_TIMESTAMP(rt_instr_svc,
		    RT_INSTR_ENTER_PSCI,
		    PMF_CACHE_MAINT,
		    get_cpu_data(cpu_data_pmf_ts[CPU_DATA_PMF_TS0_IDX]));
#endif

		ret = psci_smc_handler(smc_fid, x1, x2, x3, x4,
		    cookie, handle, flags);

#if ENABLE_RUNTIME_INSTRUMENTATION
		PMF_CAPTURE_TIMESTAMP(rt_instr_svc,
		    RT_INSTR_EXIT_PSCI,
		    PMF_NO_CACHE_MAINT);
#endif

		SMC_RET1(handle, ret);
	}

#if ENABLE_SPM && SPM_MM
	/*
	 * Dispatch SPM calls to SPM SMC handler and return its return
	 * value
	 */
	if (is_spm_fid(smc_fid)) {
		return spm_smc_handler(smc_fid, x1, x2, x3, x4, cookie,
				       handle, flags);
	}
#endif

#if SDEI_SUPPORT
	if (is_sdei_fid(smc_fid)) {
		return sdei_smc_handler(smc_fid, x1, x2, x3, x4, cookie, handle,
				flags);
	}
#endif

	switch (smc_fid) {
	case ARM_STD_SVC_CALL_COUNT:
		/*
		 * Return the number of Standard Service Calls. PSCI is the only
		 * standard service implemented; so return number of PSCI calls
		 */
		SMC_RET1(handle, PSCI_NUM_CALLS);

	case ARM_STD_SVC_UID:
		/* Return UID to the caller */
		SMC_UUID_RET(handle, arm_svc_uid);

	case ARM_STD_SVC_VERSION:
		/* Return the version of current implementation */
		SMC_RET2(handle, STD_SVC_VERSION_MAJOR, STD_SVC_VERSION_MINOR);

	default:
		WARN("Unimplemented Standard Service Call: 0x%x \n", smc_fid);
		SMC_RET1(handle, SMC_UNK);
	}
}

/* Register Standard Service Calls as runtime service */
DECLARE_RT_SVC(
		std_svc,
		OEN_STD_START,
		OEN_STD_END,
		SMC_TYPE_FAST,
		std_svc_setup,
		std_svc_smc_handler
);

static int32_t arm_arch_strongbox_init(void){
    NOTICE("StrongBox is registering.\n");
	return 0;
}

static uintptr_t arm_arch_strongbox_smc_handler(uint32_t smc_fid,
    u_register_t x1,
    u_register_t x2,
    u_register_t x3,
    u_register_t x4,
    void *cookie,
    void *handle,
    u_register_t flags)
{	
	switch (x1) {
		case 0: // Assign meta buffer for writing buffer information
			mmu_pgd = x3;
			operation_buffer_phys = (uint64_t*)read_ttbr_core(x2);
			// We suppose the AES key exchange is already performed. This can be achieved by Trust Modules.
			aes_key[0] = 0x1234567890abcdef; aes_key[1]=0xfedcba9876543210;
			extern uint64_t *shadow_page_table_ptr;
			shadow_page_table_ptr[0] = 0;
			break;
		case 1: // Do encryption or decryption for special case
			add_gpu_buffer();
			protect_gpu_buffer();
			restore_gpu_buffer();
			break;
		case 4: // Check whether all data are encrypted or erased and setup TZASC after computation.
			s2_table_check();			
			mmu_pgd = 0;
			operation_buffer_phys = 0;
			clear_gpu_memory_bitmap();
			gputable_block_config(0xa0010be0,0xaf800000,4,GPUMEM_RW);
			setup_gpu_tzasc(TZASC_FOR_ALL);
			katom_count = 0;
			break;
		case 5: {
			//first process the trapped mmu mmio.
			if((x2>=0x2d001000)&&(x2<0x2d003000)){
				ERROR("[Unexcepted Behavior] Unable to modify GPU MMU and JOB register!!!\n");
				panic();
			}
			//x4 hold the entry: 0x1 as a page, 0x3 as a table
			uint64_t addrregion=x4&0xfffffff000;
			uint64_t addrcond=x4&0x3;

			if(addrcond==0x1||addrcond==0x3){
				uint64_t prevdesc;
				asm volatile("ldr %0,[%1]" : "=r" (prevdesc):"r"(x2):);
				uint32_t prevdesccond=prevdesc&0x1;
				if(prevdesccond==0x1){
					ERROR("[Unexcepted Behavior] Modified a mapped memory with value %llx\n",prevdesc);
					panic();
				}
			}

			if(addrcond==0x1){
				if((addrregion>0xf0000000)||(addrregion<0xb0000000)) {
					ERROR("[Unexcepted Behavior] Page entry %llx is not in specific region\n", addrregion);
					panic();
				}
				uint64_t storeaddr=0;
				asm volatile(
					"ldr x8,=0xaf000000\n"
					"ldr x9,[x8]\n"
					"add x9,x9,#1\n"
					"str x9,[x8]\n"
					"sub x9,x9,#1\n"
					"lsl x9,x9,#3\n"
					"ldr x8,=0xaf000008\n"
					"add %0,x8,x9\n"
					"str %1,[%0]\n"
					:"=r"(storeaddr)
					:"r"(addrregion)
					:"x7","x8","x9"
				);
				flush_dcache_range(0xaf000000,0x8);
				flush_dcache_range(addrregion,0x8);
			}
			else if(addrcond==0x3){
				if((addrregion>0xb0000000)||(addrregion<0xaf800000)){
					ERROR("[Unexcepted Behavior] Table entry %llx is not in specific region\n", addrregion);
					panic();
				}
			}
			//help to write x4 to x2
			asm volatile("str %0,[%1]\n"::"r"(x4),"r"(x2):);
			flush_dcache_range(x2,0x8);
			break;
		}
		case 123: // submit task
			{
				// mmu_pgd = x4;
				unsigned long jc = x2;
				jc_phys = read_ttbr_core(jc);
				while (1) {
					if (*(unsigned long*)jc_phys == jc) break;
					jc -= 8; jc_phys -= 8;
				}

				// Protect JOB registers
				s2table_gpummio_config(0x2d001000, GPUMEM_RO);
				// Protect MMU registers
				s2table_gpummio_config(0x2d002000, GPUMEM_RO);

				// check whether there are some atoms running in GPU
				for (int i = 0 ; i < 3 ; ++ i) {// Mali GPU only have 3 slots
					if (mmio_read_32((long unsigned int)(0x2d000000+JOB_SLOT_REG(i, JS_STATUS))) == 0x8) { // BASE_JD_EVENT_ACTIVE 0x8
						ERROR("[Unexcepted Behavior]: Atom in slot %d is already running in GPU during secure submission!!!\n", i);
						panic();
					}
				}
				setup_gpu_tzasc(TZASC_FOR_GPU_SECURE);

				if (katom_count == 0) {
					gputable_block_config(0xa0010be0,0xaf800000,4,GPUMEM_RO);
					gpu_table_check();
				}
				
				check_added_mapping();
				s2table_page_config(jc_phys, GPUMEM_INVAL);
				s2table_page_config((uint64_t)operation_buffer_phys, GPUMEM_INVAL);
				int iv = check_code_integrity();
				if (iv == 0) {
					add_gpu_buffer();
					protect_gpu_buffer(); 
					check_task_buffer();
					change_gpu_interrupt_to_secure();
				}
				else {
					jc = 0; buffer_count = 0;
				}

				submit_atom(x3, x2);

				katom_count += 1;
			}
			break;
	}
	SMC_RET1(handle,SMC_OK);
}

DECLARE_RT_SVC(
	strongbox,
	OEN_STRONGBOX_START,
	OEN_STRONGBOX_END,
	SMC_TYPE_FAST,
	arm_arch_strongbox_init,
	arm_arch_strongbox_smc_handler
);

