#include <CL/cl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <asm-generic/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <vector>
#include <sys/time.h>

// Due to the OpenCL is close-source, we create a small lib to support StrongBox prototype.
// If the OpenCL is open-source, we can integrate these code into OpenCL.

cl_mem clCreateAlignBuffer (cl_context context,
 	cl_mem_flags flags,
 	size_t size,
 	void *host_ptr,
 	cl_int *errcode_ret);

void mark_secure();
void create_operation_buffer(cl_context context);
void write_code_hash(uint32_t *H);
void write_buffer_operation(cl_mem buffer, int decrypt, int encrypt);
void operation_end(cl_command_queue command_queue);
void encrypt_decrypt_right_now(cl_command_queue command_queue);
void tzasc_for_all(cl_command_queue command_queue);
cl_int clReleaseAlignMemObject(cl_mem mem);

#ifdef __cplusplus
extern "C" { 
#endif

void aes_enc(void *buffer, int size);
void aes_dec(void *buffer, int size);

#ifdef __cplusplus
} 
#endif
