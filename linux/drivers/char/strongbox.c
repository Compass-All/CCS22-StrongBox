#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <asm/io.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "StrongBox"

MODULE_AUTHOR("COMPASS");
MODULE_DESCRIPTION("StrongBox");
MODULE_VERSION("v1.0");
MODULE_LICENSE("GPL");

static int strongbox_driver_major;
static struct class*  strongbox_driver_class   = NULL;
static struct device* strongbox_driver_device  = NULL;

unsigned long mem_vir_addr   =   0;
unsigned long mem_phy_addr   =   0;

static int strongbox_open(struct inode * inode, struct file * filp)
{
	return 0;
}


static int strongbox_release(struct inode * inode, struct file *filp)
{
  return 0;
}


ssize_t strongbox_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    return 0;
}

extern int secure_pid;
extern void* kctx_addr;

ssize_t strongbox_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	cmd = _IOC_NR(cmd);
	u64 mmu_pgd, offset;
	switch (cmd) {
		case 0:
			offset = sizeof(struct file *) + 
					sizeof(struct kbase_device *) + 
					sizeof(u64 *) + 
					sizeof(struct mutex);
			mmu_pgd = *(unsigned long*)(kctx_addr + offset);
			asm volatile(
				"mov x1, #0\n"
				"mov x2, %0\n"
				"mov x3, %1\n"
				"ldr w0, =0xc7000001\n"
				"smc #0\n"
				:: "r"(arg), "r"(mmu_pgd)
				: "x1", "x2", "x3", "w0"
			);
			break;
		case 1:
			asm volatile(
				"mov x1, #1\n"
				"ldr w0, =0xc7000001\n"
				"smc #0\n"
				:: 
				: "x1", "w0"
			);
			break;
		case 2:
			secure_pid = current->pid;
			break;
		case 4: // De-protect page table and setup TZASC after running GPU kernel.
			asm volatile(
				"mov x1, #4\n"
				"ldr w0, =0xc7000001\n"
				"smc #0\n"
				::
				: "x1", "w0"
			);
			break;
	}
	return 0;	
}


static struct file_operations strongbox_driver_fops = {
	.owner   =    THIS_MODULE,
	.open    =    strongbox_open,
	.release =    strongbox_release,
	.read    =    strongbox_read,
	.unlocked_ioctl = strongbox_ioctl,
};


static int __init strongbox_driver_module_init(void)
{
	strongbox_driver_major = register_chrdev(0, DEVICE_NAME, &strongbox_driver_fops);
	if(strongbox_driver_major < 0){
		printk("failed to register device.\n");
		return -1;
	}
	
	
	strongbox_driver_class = class_create(THIS_MODULE, "strongbox_driver");
    if (IS_ERR(strongbox_driver_class)){
        printk("failed to create strongbox moudle class.\n");
        unregister_chrdev(strongbox_driver_major, DEVICE_NAME);
        return -1;
    }
	
	
    strongbox_driver_device = device_create(strongbox_driver_class, NULL, 
		MKDEV(strongbox_driver_major, 0), NULL, "strongbox_device");
    if (IS_ERR(strongbox_driver_device)){
        printk("failed to create device.\n");
        unregister_chrdev(strongbox_driver_major, DEVICE_NAME);
        return -1;
    }	

	printk("strongbox driver initial successfully!\n");
	
    return 0;	
}


static void __exit strongbox_driver_module_exit(void)
{
	printk("exit module.\n");
    device_destroy(strongbox_driver_class, MKDEV(strongbox_driver_major, 0));
    class_unregister(strongbox_driver_class);
	class_destroy(strongbox_driver_class);
	unregister_chrdev(strongbox_driver_major, DEVICE_NAME);
    printk("StrongBox module exit.\n");	
}

module_init(strongbox_driver_module_init);
module_exit(strongbox_driver_module_exit);
MODULE_LICENSE("GPL");