#ifndef __ASSEMBLER__

#ifndef MEMORY_UTIL
#define MEMORY_UTIL

#include <bl31/access.h>
#include <bl31/gpu_task.h>
#include <bl31/crypt.h>


void fast_memset(void *dst, unsigned long val, size_t size);
void fast_memcpy(void *dst, void *src, size_t size);
void handle_tlb_ipa(unsigned long ipa);

#endif // MEMORY_UTIL

#endif // __ASSEMBLER__


