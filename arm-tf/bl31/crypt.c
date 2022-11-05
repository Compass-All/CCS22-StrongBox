#include <bl31/access.h>
#include <bl31/gpu_task.h>
#include <bl31/memory_util.h>
#include <bl31/crypt.h>
#include <bl31/strongbox_defs.h>

uint32_t init_RC[10] = {0x01000000,0x02000000,0x04000000,0x08000000,0x10000000,0x20000000,0x40000000,0x80000000,0x1b000000,0x36000000};
uint32_t init_H[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

uint32_t Sbox[256] = {
	0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
	0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
	0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
	0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
	0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
	0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
	0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
	0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
	0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
	0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
	0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
	0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
	0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
	0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
	0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
	0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

void key_expansion(uint32_t *kv) {
	*(uint64_t*)kv = aes_key[0]; *(((uint64_t*)kv) + 1) = aes_key[1];
	int indekz=4;

	uint32_t ktv;
	int cnt=0;
	while(indekz<44){
		if(indekz%4==0){
			ktv=((kv[indekz-1]<<8)&(0xffffffff))|((kv[indekz-1]>>24)&(0xffffffff));//rotate
			//subbytes
			uint32_t tmp78=(ktv&0xff000000)>>24;
			uint32_t tmp56=(ktv&0x00ff0000)>>16;
			uint32_t tmp34=(ktv&0x0000ff00)>>8;
			uint32_t tmp12=(ktv&0x000000ff)>>0;
			tmp78=Sbox[tmp78];
			tmp56=Sbox[tmp56];
			tmp34=Sbox[tmp34];
			tmp12=Sbox[tmp12];
			ktv=(tmp78<<24)+(tmp56<<16)+(tmp34<<8)+tmp12;
			ktv=ktv^init_RC[cnt];//eor with rc
			kv[indekz]=kv[indekz-4]^ktv;
			cnt+=1;
		}
		else{
			kv[indekz]=kv[indekz-1]^kv[indekz-4];
		}
		indekz+=1;
	}
}

void decrypt_buffer(struct gpu_buffer* buffer) {
	uint32_t kv[44];
	if (buffer->flag & 0x1) key_expansion(kv);

	uint32_t H[8];
	if ((buffer->flag & 0x1) || (buffer->flag & 0x4)) fast_memcpy(H, init_H, sizeof(H));

	int offset = 0;
	int thisstepsize = 0;
	while (offset < buffer->size) {
		void *current_ptr = (void*)read_ttbr_core((uint64_t)buffer->virt_base + offset);
		offset += 4096;
		if (offset > buffer->size) thisstepsize = buffer->size - (offset-4096);
		else thisstepsize = 4096;
		flush_dcache_range((uint64_t)(current_ptr), thisstepsize); 
		s2table_page_config((uint64_t)current_ptr, GPUMEM_INVAL); // always protect page
		if (buffer->flag & 0x1) { // conditionally do decryption
			aes128_block(kv, current_ptr, thisstepsize, 1);
		}
		if ((buffer->flag & 0x1) || (buffer->flag & 0x4)) { // 0x2 only protect, no integrity check
			if (offset < buffer->size) sha256(H, current_ptr, thisstepsize);
			else sha256_final(H, current_ptr, thisstepsize, buffer->size);
		}
		flush_dcache_range((uint64_t)(current_ptr), thisstepsize); // for GPU
    }
	// Then we can compare the hash values with operation buffer
}

void encrypt_buffer(struct gpu_buffer* buffer){
	uint32_t kv[44];
	uint32_t H[8];
	if (buffer->flag & 0x8) {
		key_expansion(kv);
		fast_memcpy(H, init_H, sizeof(H));
	}

	int offset = 0;
	int thisstepsize = 0;
	while (offset < buffer->size) {
		void *current_ptr = (void*)read_ttbr_core((uint64_t)buffer->virt_base + offset);
		offset += 4096;
		if (offset > buffer->size) thisstepsize = buffer->size - (offset-4096);
		else thisstepsize = 4096;
		flush_dcache_range((uint64_t)(current_ptr), thisstepsize); 
		if (buffer->flag & 0x8) {
			if (offset < buffer->size) sha256(H, current_ptr, thisstepsize);
			else sha256_final(H, current_ptr, thisstepsize, buffer->size);
			aes128_block(kv, current_ptr, thisstepsize, 0);
		}
		else { // 0x10
			fast_memset(current_ptr, 0, thisstepsize); 
		}
		flush_dcache_range((uint64_t)(current_ptr), thisstepsize); 
		s2table_page_config((uint64_t)current_ptr, GPUMEM_RW);
    }
	// Then we can save the hash values to operation buffer
}

void protect_gpu_buffer(){
	int i = 0;
	for (i = 0; i < buffer_count; ++ i) {
		if (gpu_buffer[i].flag & 0x7) { // 0b 0111
			decrypt_buffer(&gpu_buffer[i]);
		}
	}
}

void restore_gpu_buffer(){
	int i = 0;
	for (i = 0; i < buffer_count; i ++) {
		if (gpu_buffer[i].flag & 0x18) { // 0b 1 1000
			encrypt_buffer(&gpu_buffer[i]);
		}
	}
}

void sha256(uint32_t *ctx, const void *in, size_t size) {
	size_t block_num = size / 64;
	if (block_num) sha256_block_data_order(ctx, in, block_num); 
}

void sha256_final(uint32_t *ctx, const void *in, size_t remain_size, size_t tot_size) {
	size_t block_num = remain_size / 64;
	sha256(ctx, in, block_num*64);

	size_t remainder = remain_size % 64;
	size_t tot_bits = tot_size * 8;
	char last_block[64]; 
	fast_memset(last_block, 0, sizeof(last_block));
	fast_memcpy(last_block, (void*)in+block_num*64, remainder);
	last_block[remainder] = 0x80;
	if (remainder < 56) {}
	else {
		sha256_block_data_order(ctx, last_block, 1);
		fast_memset(last_block, 0, sizeof(last_block));
	}
	for (int i = 0 ; i < 8 ; ++ i) last_block[63-i] = tot_bits >> (i * 8);
	sha256_block_data_order(ctx, last_block, 1);
}

void hmac_sha256(void *out, const void *in, size_t size) {
	char k[64], k_ipad[64], k_opad[64];
	fast_memset(k, 0, 64);
	fast_memcpy(k, aes_key, 32);
	for (int i = 0 ; i < 64 ; ++ i) {
		k_ipad[i] ^= k[i];
		k_opad[i] ^= k[i];
	}
	uint32_t ihash[8]; fast_memcpy(ihash, init_H, sizeof(ihash));
	sha256(ihash, k_ipad, 64); 
	sha256(ihash, in, size); 
	sha256_final(ihash, k_ipad, size%64, size+64);
	uint32_t ohash[8]; fast_memcpy(ohash, init_H, sizeof(ohash));
	sha256(ohash, k_opad, 64); 
	sha256(ohash, ihash, 64);
	sha256_final(ohash, k_opad, 0, 128);
	fast_memcpy(out, ohash, 64);
}

int parse_mali_instruction(uint64_t *code) {
	// refer: https://gitlab.freedesktop.org/panfrost/mali-isa-docs/-/blob/master/Midgard.md
	// 3 - Texture (4 words)
	// 5 - Load/Store (4 words)
	// 8 - ALU (4 words)
	// 9 - ALU (8 words)
	// A - ALU (12 words)
	// B - ALU (16 words)
	uint64_t code_start = (uint64_t)code;
	// int code_length = 0;
	int current_type, next_type;
	while (1) {
		current_type = ((*code) & 0xf);
		next_type    = ((*code) & 0xf0) >> 4;
		switch (current_type) {
			case 3:
				code += 2;
				break;
			case 4:
				code += 2;
				break;
			case 5:
				code += 2;
				break;
			case 8:
				code += 2;
				break;
			case 9:
				code += 4;
				break;
			case 0xa:
				code += 6;
				break;
			case 0xb:
				code += 8;
				break;
			default:
				ERROR("[Unexcepted Behavior]: Instruction format [%d] error!\n", current_type);
				panic();
		}
		
		if (next_type == 1) break;
	}
	int code_length = (uint64_t)(code) - code_start;
	return code_length;
}

int check_code_integrity() {
    void *code_seg = (void*)(*(uint64_t*)(jc_phys+0x138));
	void *code_ptr = (void*)(*(uint64_t*)read_ttbr_core((uint64_t)code_seg));
	void *code = (void *)(read_ttbr_core((uint64_t)code_ptr) & (~0xf));
	s2table_page_config((uint64_t)code, GPUMEM_INVAL);
	int code_length = parse_mali_instruction(code);

	uint32_t HMAC[8]; 
	hmac_sha256(HMAC, code, code_length);
	uint32_t H0[8];
	fast_memcpy(H0, operation_buffer_phys, 32);
	// TODO: If we locate the GPU code in closed-source OpenCL, then we can compare the HAMC value in operation_buffer
	if (memcmp(HMAC, H0, 32) != 0) {
		// ERROR("HMAC value does not match!!!\n");
		// panic();
	}
    return 0;
}