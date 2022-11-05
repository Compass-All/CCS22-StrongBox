
#ifndef __ASSEMBLER__

#ifndef GPUTASK
#define GPUTASK

#include <drivers/arm/gicv2.h>
#include <drivers/arm/tzc400.h>
#include <plat/arm/common/arm_def.h>
#include <plat/arm/common/plat_arm.h>
#include <drivers/arm/tzc_common.h>

void check_task_buffer();
int add_gpu_buffer();
void submit_atom(int js, uint64_t jc_head);
void juno_irq_mali_job_handler();
void change_gpu_interrupt_to_non_secure();
void change_gpu_interrupt_to_secure();

#endif // GPUTASK

#endif // __ASSEMBLER__

