#include <bl31/access.h>
#include <bl31/gpu_task.h>
#include <bl31/memory_util.h>
#include <bl31/crypt.h>
#include <bl31/strongbox_defs.h>

void fast_memset(void *dst, unsigned long val, size_t size) {
	size_t _size = size / 64 * 64;
	for (unsigned long addr = (unsigned long)dst ; addr < (unsigned long)dst + _size ; addr += 64) {
		((unsigned long*)addr)[0] = val; ((unsigned long*)addr)[1] = val; ((unsigned long*)addr)[2] = val; ((unsigned long*)addr)[3] = val;
		((unsigned long*)addr)[4] = val; ((unsigned long*)addr)[5] = val; ((unsigned long*)addr)[6] = val; ((unsigned long*)addr)[7] = val;
	}
	memset(dst+_size, val, size-_size);
}
void fast_memcpy(void *dst, void *src, size_t size) {
	for (long long remain = size ; remain-32 >= 0 ; dst += 32, src += 32, remain -= 32) {
		((unsigned long*)dst)[0] = ((unsigned long*)src)[0]; ((unsigned long*)dst)[1] = ((unsigned long*)src)[1];
		((unsigned long*)dst)[2] = ((unsigned long*)src)[2]; ((unsigned long*)dst)[3] = ((unsigned long*)src)[3];
	}
}

void handle_tlb_ipa(unsigned long ipa){
	unsigned long chunkipa=ipa>>12;
	asm volatile(
		"dsb ish\n"
		"tlbi ipas2e1is, %0\n"
		"dsb ish\n"
		"tlbi vmalle1is\n"
		"dsb ish\n"
		"isb\n"
		:
		:"r"(chunkipa)
		:
	);
}