#ifndef __ASSEMBLER__

#ifndef ACCESS
#define ACCESS

#include <drivers/arm/gicv2.h>
#include <drivers/arm/tzc400.h>
#include <plat/arm/common/arm_def.h>
#include <plat/arm/common/plat_arm.h>
#include <drivers/arm/tzc_common.h>

#include <bl31/strongbox_defs.h>
#include <bl31/crypt.h>
#include <bl31/gpu_task.h>
#include <bl31/memory_util.h>

#define GPUMEM_INVAL 0x0
#define GPUMEM_WO 0x2
#define GPUMEM_RO 0x1
#define GPUMEM_RW (GPUMEM_WO | GPUMEM_RO)


#define IS_VALID(entry) (get_bit_range_value(entry,0,0) == 1)
#define IS_BLOCK(entry) (get_bit_range_value(entry,1,1) == 0)
#define IS_TABLE(entry) (get_bit_range_value(entry,1,1) == 1)
#define IS_RESERVED(entry) (get_bit_range_value(table_base,1,0) == 0x01)

#define get_bit_range_value(number, start, end) (( (number) >> (end) ) & ( (1L << ( (start) - (end) + 1) ) - 1) )
#define GET_BITMAP_ADDR(phys_addr) ((( (phys_addr&0xfffff000) - 0xb0000000 ) >> 9 ) + 0xaf600000)
#define gpu_memory_bitmap_config(phys_addr, bit) *(uint64_t*)GET_BITMAP_ADDR(phys_addr) = bit
#define gpu_memory_bitmap_check(phys_addr) *(uint64_t*)GET_BITMAP_ADDR(phys_addr)

#define SHADOW_PAGE_TABLE_SIZE 16384 
#define SHADOW_PAGE_TABLE_NUMBER 64



void gpu_table_check();
void clear_gpu_memory_bitmap();
void s2_table_check();
void s2table_gpummio_config(uint64_t physaddr,uint32_t attr);
void s2table_gpummu_config(uint64_t physaddr,uint32_t attr);
void s2table_page_config(uint64_t physaddr,uint32_t attr);

void gputable_block_config(uint64_t s2addr, uint64_t gputableaddr, uint32_t blocknum,uint32_t attr);
uint64_t read_ttbr_core(uint64_t IA);

int is_ptr(unsigned long long ptr);
int in_reserved_memory(unsigned long long addr);
void setup_gpu_tzasc(int flag);
void check_added_mapping();


#endif // ACCESS

#endif // __ASSEMBLER__
