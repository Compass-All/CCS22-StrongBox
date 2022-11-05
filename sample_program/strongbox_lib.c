#include "strongbox_lib.h"
#include <map>

// Due to the OpenCL is close-source, we create a small lib to support StrongBox prototype.
// If the OpenCL is open-source, we can integrate these code into OpenCL.

cl_mem operation_buffer;
int buffer_count;
unsigned long operation_array[512]; 

std::map<unsigned long, std::pair<unsigned long, unsigned long> > buffers;
size_t get_buffer_size(cl_mem mem) {
    unsigned long addr = (unsigned long)mem;
    return buffers[addr].first;
}

#define CMD(x) _IO(77, x)

void mark_secure() {
    int fd = open("/dev/strongbox_device", O_RDWR);
    int ioctlret = ioctl(fd, CMD(2), 0);
    assert(fd != -1 && ioctlret == 0);
    close(fd);
}

cl_mem clCreateAlignBuffer (cl_context context,
 	cl_mem_flags flags,
 	size_t size,
 	void *host_ptr,
 	cl_int *errcode_ret)
{
    size_t actual_size = size+4096;
    cl_mem ret = clCreateBuffer(context, flags, actual_size, host_ptr, errcode_ret);
    unsigned long addr = *(unsigned long*)((char*)ret+368);
    unsigned long align_addr = addr % 0x1000 ? (addr/0x1000 + 1) * 0x1000 : addr;
    if (addr != align_addr) {
        *(unsigned long*)((char*)ret+368) = align_addr; 
        unsigned long ptr;
        ptr = *(unsigned long*)((char*)ret+376);
        ptr = *(unsigned long*)(ptr+0x80);
        assert(
            *(unsigned long*)(ptr+0x10) == addr && 
            *(unsigned long*)(ptr+0x20) == addr && 
            *(unsigned long*)(ptr+0x30) == addr
        );
        *(unsigned long*)(ptr+0x10) = align_addr;
        *(unsigned long*)(ptr+0x20) = align_addr;
        *(unsigned long*)(ptr+0x30) = align_addr;
    }
    buffers.insert(std::make_pair((unsigned long)ret, std::make_pair(size, addr)));
    return ret;
}

cl_int clReleaseAlignMemObject(cl_mem mem) {
    unsigned long current_addr = *(unsigned long*)((char*)mem+368);
    unsigned long origin_addr = buffers[(unsigned long)mem].second;
    // recover
    *(unsigned long*)((char*)mem+368) = origin_addr; 
    unsigned long ptr;
    ptr = *(unsigned long*)((char*)mem+376);
    ptr = *(unsigned long*)(ptr+0x80);
    *(unsigned long*)(ptr+0x10) = origin_addr;
    *(unsigned long*)(ptr+0x20) = origin_addr;
    *(unsigned long*)(ptr+0x30) = origin_addr;

    buffers.erase((unsigned long)mem);
    return clReleaseMemObject(mem);
}

cl_int clEnqueueReadAlignBuffer(
    cl_command_queue command_queue,
    cl_mem buffer,
    cl_bool blocking_read,
    size_t offset,
    size_t size,
    void* ptr,
    cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list,
    cl_event* event)
{
    return clEnqueueReadBuffer(command_queue, buffer, blocking_read, 
        offset, size, ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int clEnqueueWriteAlignBuffer(
    cl_command_queue command_queue,
    cl_mem buffer,
    cl_bool blocking_write,
    size_t offset,
    size_t size,
    const void* ptr,
    cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list,
    cl_event* event)
{
    return clEnqueueWriteBuffer(command_queue, buffer, blocking_write, 
        offset, size, ptr, num_events_in_wait_list, event_wait_list, event);
}

// Call this function to create [operation_buffer]
void create_operation_buffer(cl_context context) {
    cl_int errcode_ret;
    operation_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, 4096, NULL, &errcode_ret);
    assert(operation_buffer != NULL);
    int fd = open("/dev/strongbox_device", O_RDWR);
    uint64_t virt_base = *(uint64_t*)((char*)operation_buffer+368);
    int ret = ioctl(fd, CMD(0), virt_base);
    assert(fd != -1 && ret == 0);
    close(fd);
    buffer_count = 0;
}

void write_code_hmac(uint32_t *HMAC) {
    memcpy(operation_array, HMAC, 32);
}

// Call this function to write GPU buffer operation in [operation_buffer]
// We can extend this function to write the GPU buffer hash value in [operation_buffer]
void write_buffer_operation(cl_mem buffer, int protect_decrypt, int encrypt_restore) {
    size_t size = get_buffer_size(buffer);

    int index = buffer_count;
    unsigned long flag = 0;
    
    if (protect_decrypt & 0x1) { // decrypt && protect
        flag |= 1;
    }
    else if (protect_decrypt & 0x2) { // only protect, not decrypt
        flag |= 2;
    }
    else if (protect_decrypt & 0x4) { // only protect & integrity check, not decrypt
        flag |= 4;
    }

    if (encrypt_restore & 0x1) { // encrypt the data after computation
        flag |= 8;
    }
    else if (encrypt_restore & 0x2) { // erase the data after computation
        flag |= 16;
    }
    unsigned long* current_operation_array = operation_array + 4 + index*3; // head 32 bytes of operation_buffer are HMAC of code
    current_operation_array[0] = *(unsigned long*)((char*)buffer+368);
    current_operation_array[1] = size;
    current_operation_array[2] = flag;
    ++ buffer_count;
}

// Call this function before clEnqueueNDRangeKernel()
void operation_end(cl_command_queue command_queue) {
    int index = buffer_count;
    // Head 32 bytes of [operation_buffer] stores HMAC of code
    // In our sample program, we do not provide the code HAMC since locate the code location in here require extra reverse engineering effort
    // write_code_hmac(HMAC);
    operation_array[3*index+4] = 0; 
    clEnqueueWriteBuffer(command_queue, 
            operation_buffer, 
            1, 
            0, 
            32 + 3*index*8 + 8, 
            operation_array, 
            0, NULL, NULL);
    buffer_count = 0;
}

void encrypt_decrypt_right_now(cl_command_queue command_queue) {
    clFinish(command_queue);
    int fd = open("/dev/strongbox_device", O_RDWR);
    ioctl(fd, CMD(1), 0);
    close(fd);
}

void tzasc_for_all(cl_command_queue command_queue) {
    clFinish(command_queue);
    int fd = open("/dev/strongbox_device", O_RDWR);
    int ret = ioctl(fd, CMD(4), 0);
    assert(fd != -1 && ret == 0);
    close(fd);
}

#ifdef __cplusplus
extern "C" { 
#endif

void aes128_block(uint32_t *expandedkeys, uint64_t *data, size_t size, uint32_t isdec);

uint32_t kv[44];

uint32_t init_RC[10] = {0x01000000,0x02000000,0x04000000,0x08000000,0x10000000,0x20000000,0x40000000,0x80000000,0x1b000000,0x36000000};

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

//Assumed this key is exchanged through Trust Modules.
uint64_t aes_key[2] = {
    0x1234567890abcdef, 0xfedcba9876543210
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

void aes_enc(void* buf, int size) {
    key_expansion(kv);
    aes128_block(kv, (uint64_t*)buf, size, 0);
}

void aes_dec(void* buf, int size) {
    key_expansion(kv);
    aes128_block(kv, (uint64_t*)buf, size, 1);
}

#ifdef __cplusplus
} 
#endif
