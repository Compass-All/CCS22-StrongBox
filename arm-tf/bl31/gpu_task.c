#include <bl31/access.h>
#include <bl31/gpu_task.h>
#include <bl31/memory_util.h>
#include <bl31/crypt.h>
#include <bl31/strongbox_defs.h>

void check_task_buffer() {
	uint64_t data_field_addr = read_ttbr_core(*(uint64_t*)(jc_phys+0x130)); 
	uint64_t code_field_addr = read_ttbr_core(*(uint64_t*)(jc_phys+0x138)); 
	for (uint64_t current_addr = data_field_addr ; current_addr < code_field_addr ; current_addr += 8) {
		uint64_t data_addr = *(uint64_t*)current_addr;
		uint64_t phys_addr = read_ttbr_core(data_addr);
		if (phys_addr != 0) {
			uint64_t s2_entry_addr = 0xa0200000 + ((phys_addr-0xb0000000) >> 12) * 8;
			if ( ( *(uint64_t*)s2_entry_addr & 0x1 ) != 0 ) {
				ERROR("[Unexcepted Behavior]: Use GPU buffer without protection!!!\n");
				panic();
			}
		}
	}
}

int add_gpu_buffer() {
	uint64_t* buffer_info = operation_buffer_phys + 4; // head 32 bytes are HAMC of code
	buffer_count = 0;
	while (*buffer_info != 0) {
		uint64_t hash_flag = buffer_info[2];
		if (hash_flag) {
			gpu_buffer[buffer_count].virt_base = (void*)buffer_info[0];
			if ((uint64_t)gpu_buffer[buffer_count].virt_base % 0x1000 != 0) {
				ERROR("[Unexcepted Behavior] Buffer 0x%llx is not align!!!\n", (uint64_t)gpu_buffer[buffer_count].virt_base);
				panic();
			}
			gpu_buffer[buffer_count].size = buffer_info[1];
			gpu_buffer[buffer_count].flag = hash_flag & 0xffffffff;
			gpu_buffer[buffer_count].hash = hash_flag >> 32;
			buffer_count ++;
		}
		buffer_info += 3;
	}
	return 0;
}

void submit_atom(int js, uint64_t jc_head) {
	as_nr = mmio_read_32((long unsigned int)(GPU_MMIO_BASE + JOB_SLOT_REG(js, JS_CONFIG_NEXT))) & 0xf;

	uint64_t current_mmu_pgd;
	current_mmu_pgd = mmio_read_32((long unsigned int)(GPU_MMIO_BASE + MMU_AS_REG(as_nr, AS_TRANSTAB_LO))) & 0xfffff000;
	current_mmu_pgd |= (uint64_t)mmio_read_32((long unsigned int)(GPU_MMIO_BASE + MMU_AS_REG(as_nr, AS_TRANSTAB_HI))) << 32;
	if (current_mmu_pgd != mmu_pgd) {
		ERROR("[Unexcepted Behavior] GPU page table base is changed!!!\n");
		panic();
	}
	mmio_write_32((long unsigned int)(GPU_MMIO_BASE+JOB_SLOT_REG(js, JS_HEAD_NEXT_LO)), jc_head & 0xFFFFFFFF);
	mmio_write_32((long unsigned int)(GPU_MMIO_BASE+JOB_SLOT_REG(js, JS_HEAD_NEXT_HI)), jc_head >> 32);
	mmio_write_32((long unsigned int)GPU_MMIO_BASE+JOB_SLOT_REG(js, JS_COMMAND_NEXT), JS_COMMAND_START);
}

void change_gpu_interrupt_to_secure() {
    gicv2_set_interrupt_type(65, GICV2_INTR_GROUP0);
    return;
}

void change_gpu_interrupt_to_non_secure() {
    gicv2_set_interrupt_type(65, GICV2_INTR_GROUP1);
    return;
}

void juno_irq_mali_job_handler() {
	if (jc_phys != 0)
	{
		unsigned int mali_job_group;
		mali_job_group = gicv2_get_interrupt_group(65);

		// secure group
		if (mali_job_group == GICV2_INTR_GROUP0) {

			void *code_seg = (void*)(*(unsigned long long*)(jc_phys+0x138));
			void *code_ptr = (void*)(*(unsigned long long*)read_ttbr_core((unsigned long long)code_seg));
			void *code = (void *)(read_ttbr_core((unsigned long)code_ptr) & (~0xfff));
			restore_gpu_buffer(); 
			s2table_page_config((unsigned long)code, GPUMEM_RW);
			s2table_page_config(jc_phys, GPUMEM_RW);
			s2table_page_config((uint64_t)operation_buffer_phys, GPUMEM_RW);
			setup_gpu_tzasc(TZASC_FOR_SECURE_ONLY);

			// Restore JOB registers
			s2table_gpummio_config(0x2d001000, GPUMEM_RW);
			// Restore MMU registers
			s2table_gpummio_config(0x2d002000, GPUMEM_RW);
			
			change_gpu_interrupt_to_non_secure();
			jc_phys = 0;
			buffer_count = 0;
		}

		mali_job_group = gicv2_get_interrupt_group(65);
	}
}