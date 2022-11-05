#ifndef __ASSEMBLER__

#ifndef STRONGBOX_CRYPT
#define STRONGBOX_CRYPT

#include <drivers/arm/gicv2.h>
#include <drivers/arm/tzc400.h>
#include <plat/arm/common/arm_def.h>
#include <plat/arm/common/plat_arm.h>
#include <drivers/arm/tzc_common.h>

#include <bl31/access.h>
#include <bl31/gpu_task.h>
#include <bl31/memory_util.h>

/* Write the AES SHA function prototype in here. */
void decrypt_buffer(struct gpu_buffer* buffer);
void encrypt_buffer(struct gpu_buffer* buffer);
void protect_gpu_buffer();
void restore_gpu_buffer();
void key_expansion(uint32_t *kv);
void aes128_block(uint32_t *expandedkeys, uint64_t *data, size_t size, uint32_t isdec);

void sha256(uint32_t *ctx, const void *in, size_t size);
void sha256_final(uint32_t *ctx, const void *in, size_t remain_size, size_t tot_size);
void sha256_block_data_order(uint32_t *ctx, const void *in, size_t num);
int check_code_integrity();
/* Write the AES SHA function prototype in here. */

#endif
#endif // __ASSEMBLER__