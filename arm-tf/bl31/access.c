#include <bl31/access.h>
#include <bl31/gpu_task.h>
#include <bl31/memory_util.h>
#include <bl31/crypt.h>
#include <bl31/strongbox_defs.h>

#define PAGE_TABLE_NOT_IN_RANGE(addr) (!(0xaf800000 <= addr && addr < 0xb0000000))
#define MEMORY_NOT_IN_RANGE(addr) (!(0xb0000000 <= addr && addr < 0xc0000000))
uint64_t *shadow_page_table_ptr = (uint64_t*)0xaf000000;
uint64_t *gpu_memory_bitmap = (uint64_t*)0xaf600000;

void s2table_gpummu_config(uint64_t physaddr,uint32_t attr){
	if (physaddr > 0xffffffff) {
		ERROR("[Unexcepted Behavior] Out of bound: 0x%llx\n", physaddr);
		panic();
		}
	uint64_t s2descaddr=(((physaddr&0xfffff000)-0x2b400000)>>9)+0xa000b000;
	uint64_t s2descval;
	if(attr==GPUMEM_INVAL){
		s2descval=0;
	}
	else{
		s2descval=((physaddr&0xfffff000)|0x73f|(attr<<6));
	}
	asm volatile("str %0,[%1]\n"::"r"(s2descval),"r"(s2descaddr):);
	flush_dcache_range(s2descaddr,0x8);
	handle_tlb_ipa(physaddr);
	return;
}

void s2table_gpummio_config(uint64_t physaddr,uint32_t attr){
	if (physaddr > 0xffffffff) {
		ERROR("[Unexcepted Behavior] Out of bound: 0x%llx\n", physaddr);
		panic();
	}
	uint64_t s2descaddr=(((physaddr&0xfffff000)-0x2d000000)>>9)+0xa0040000;
	uint64_t s2descval;
	if(attr==GPUMEM_INVAL){
		s2descval=0;
	}
	else{
		s2descval=((physaddr&0xfffff000)|0x73f|(attr<<6));
	}
	asm volatile("str %0,[%1]\n"::"r"(s2descval),"r"(s2descaddr):);
	flush_dcache_range(s2descaddr,0x8);
	handle_tlb_ipa(physaddr);
	return;
}

void s2table_page_config(uint64_t physaddr,uint32_t attr){
	if (physaddr > 0xffffffff) {
		ERROR("[Unexcepted Behavior] Out of bound: 0x%llx\n", physaddr);
		panic();
	}
	uint64_t s2descaddr=(((physaddr&0xfffff000)-0xb0000000)>>9)+0xa0200000;
	uint64_t s2descval;
	if(attr==GPUMEM_INVAL){
		s2descval=0;
	}
	else{
		s2descval=((physaddr&0xfffff000)|0x73f|(attr<<6));
	}

	asm volatile("str %0,[%1]\n"::"r"(s2descval),"r"(s2descaddr):);
	flush_dcache_range(s2descaddr,0x8);
	handle_tlb_ipa(physaddr);
	return;
}

void clear_gpu_memory_bitmap() {
	fast_memset(gpu_memory_bitmap, 0, 0x200000);
}

void traverse_gpu_page_table(uint64_t current_base, int level) {
	for (uint64_t ptr = current_base ; ptr < current_base+0x1000 ; ptr += 8) {
		uint64_t addr = *(uint64_t*)ptr;
		if (addr & 0x1) {
			if (level < 3) {
				if (addr & 0x1) traverse_gpu_page_table(addr & (~0xfff), level+1);
			}
			else {
				if (addr & 0x1) {
					addr &= 0xfffffffff000;
					if ( gpu_memory_bitmap_check(addr) == 0 ) gpu_memory_bitmap_config(addr, 1);
					else {
						ERROR("Duplicate mapping!!! addr = 0x%llx\n", addr);
						panic();
					}
				}
			}
		}
	}
}

void gpu_table_check() {
	traverse_gpu_page_table(mmu_pgd & (~0xfff), 0);
}

void s2_table_check(){
	for (unsigned long addr = 0xa0200000 ; addr < 0xa0400000 ; addr += 8) {
		if ( (*(unsigned long*)addr & 0x1) == 0 ) {
			ERROR("Table check error!!!\n");
			panic();
		}
	}
}

//example:gputable_block_config(0xa0010be0,0xaf800000,4,GPUMEM_RW);
//example:gputable_block_config(0xa0010be0,0xaf800000,4,GPUMEM_RO);
void gputable_block_config(uint64_t s2addr, uint64_t gputableaddr, uint32_t blocknum,uint32_t attr){
	// block desc: 0111_WR11_1101
	uint32_t curnum=0;
	while(curnum<blocknum){
		uint64_t curdesc=(gputableaddr|0x73d|(attr<<6))+(curnum<<21);
		uint64_t curaddr=s2addr+(curnum<<3);
		asm volatile("str %0,[%1]\n"::"r"(curdesc),"r"(curaddr):);
		curnum+=1;
	}
	flush_dcache_range(s2addr,blocknum<<3);
}

uint64_t read_ttbr_core(uint64_t IA) {
	uint64_t offset, phys_DA, table_base, OA;
	table_base = mmu_pgd;
	offset = get_bit_range_value(IA, 47, 39) << 3;
	phys_DA = (table_base & 0xfffffffff000) | offset;
	if (PAGE_TABLE_NOT_IN_RANGE(phys_DA)) {
		ERROR("[Unexpected Behavior] Not in range entry: 0x%llx\n", phys_DA);
		panic();
	}
	table_base = *(uint64_t*)phys_DA;
	if ((table_base & 0x1) == 0) return 0;
	offset = get_bit_range_value(IA, 38, 30) << 3;
	phys_DA = (table_base & 0xfffffffff000) | offset;
	if (PAGE_TABLE_NOT_IN_RANGE(phys_DA))  {
		ERROR("[Unexpected Behavior] Not in range entry: 0x%llx\n", phys_DA);
		panic();
	}
	table_base = *(uint64_t*)phys_DA;
	if ((table_base & 0x1) == 0) return 0;
	offset = get_bit_range_value(IA, 29, 21) << 3;
	phys_DA = (table_base & 0xfffffffff000) | offset;
	if (PAGE_TABLE_NOT_IN_RANGE(phys_DA))   {
		ERROR("[Unexpected Behavior] Not in range entry: 0x%llx\n", phys_DA);
		panic();
	}
	table_base = *(uint64_t*)phys_DA;
	if ((table_base & 0x1) == 0) return 0;
	offset = get_bit_range_value(IA, 20, 12) << 3;
	phys_DA = (table_base & 0xfffffffff000) | offset;
	if (PAGE_TABLE_NOT_IN_RANGE(phys_DA))  {
		ERROR("[Unexpected Behavior] Not in range entry: 0x%llx\n", phys_DA);
		panic();
	}
	table_base = *(uint64_t*)phys_DA;
	if ((table_base & 0x1) == 0) return 0;
	offset = IA & 0xfff;
	OA = (table_base & 0xfffffffff000) | offset;
	if (MEMORY_NOT_IN_RANGE(OA))   {
		ERROR("[Unexpected Behavior] Not in range entry: 0x%llx\n", phys_DA);
		panic();
	}
	return OA;
}

// 0          virt_addr
// 1          size
// 2...16383  phys_addr
// 8 * 16384 = 1 << 17
// total: 8M = 1 << 23
// entry: 1 << 6 = 64

void check_added_mapping() {
	uint64_t count = *shadow_page_table_ptr;
	uint64_t *ptr = shadow_page_table_ptr + 1;
	for (int i = 0 ; i < count ; ++ i) {
		if (gpu_memory_bitmap_check(ptr[i]) != 0) {
			ERROR("Add Illegal Mapping!!!\n");
			panic();
		}
	}
	*shadow_page_table_ptr = 0;
}

static inline void write_region_nsaid(int region, uint32_t nsaid){
	mmio_write_32(PLAT_ARM_TZC_BASE + 0x8, 0x0);
	mmio_write_32(PLAT_ARM_TZC_BASE + REGION_REGISTER_OFFSET(ID_ACCESS,region), nsaid);
	mmio_write_32(PLAT_ARM_TZC_BASE + 0x8, 0xf);
}

void setup_gpu_tzasc(int flag) {
	tzc400_disable_filters();
	if (flag == TZASC_FOR_ALL) {
		write_region_nsaid(2, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(4, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(5, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(6, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(7, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(8, PLAT_ARM_TZC_NS_DEV_ACCESS);
	}
	else if (flag == TZASC_FOR_SECURE_ONLY) {
		write_region_nsaid(2, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(4, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(5, TZC_REGION_ACCESS_RDWR(TZC400_NSAID_AP));
		write_region_nsaid(6, TZC_REGION_ACCESS_RDWR(TZC400_NSAID_AP)|TZC_REGION_ACCESS_WR(TZC400_NSAID_DMA330));
		write_region_nsaid(7, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(8, PLAT_ARM_TZC_NS_DEV_ACCESS);
	}
	if (flag == TZASC_FOR_GPU_SECURE) { 
		write_region_nsaid(2, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(4, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(5, TZC_REGION_ACCESS_RDWR(TZC400_NSAID_AP)|TZC_REGION_ACCESS_RD(TZC400_NSAID_GPU));
		write_region_nsaid(6, TZC_REGION_ACCESS_RDWR(TZC400_NSAID_AP)|TZC_REGION_ACCESS_RDWR(TZC400_NSAID_GPU));
		write_region_nsaid(7, PLAT_ARM_TZC_NS_DEV_ACCESS);
		write_region_nsaid(8, PLAT_ARM_TZC_NS_DEV_ACCESS);
	}
	tzc400_enable_filters();
}