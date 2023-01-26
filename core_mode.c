#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioctl.h>
#include <linux/ptrace.h>
#include <linux/pid.h>
#include <linux/netdevice.h>
#include <linux/mm_types.h>
#include <asm/page.h>
#include <linux/fs.h>
#include <asm/processor.h>
#include <linux/namei.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#define RD_MY_THREAD_STRUCT _IOWR('a','a', struct thread_struct_request*)
#define WR_PID _IOW('a','a', int32_t*)
#define RD_MY_PAGE _IOR('a','c', struct my_page*)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My ioctl driver");
MODULE_VERSION("1.0");

const int MYMAJOR = 28;

int pid = 0;

struct my_seccomp_data {
	int nr;
	__u32 arch;
	__u64 instruction_pointer;
	__u64 args[6];
};

struct my_page {
	unsigned long flags;
	void* mapping;
	long vm_address;
};

struct my_thread_struct {
	/* Cached TLS descriptors: */
    unsigned long        sp;
    unsigned short        es;
    unsigned short        ds;
    unsigned short        fsindex;
    unsigned short        gsindex;
 
    unsigned long        fsbase;
    unsigned long        gsbase;
 
    /* Debug status used for traps, single steps, etc... */
    unsigned long           debugreg6;
    /* Keep track of the exact dr7 value set by the user */
    unsigned long           ptrace_dr7;
    /* Fault info: */
    unsigned long        cr2;
    unsigned long        trap_nr;
    unsigned long        error_code;
    /* IO permissions: */
    unsigned long        *io_bitmap_ptr; // from ptr to ul
    unsigned long        iopl;
    /* Max allowed port in the bitmap, in bytes: */
    unsigned        io_bitmap_max;
};

struct thread_struct_request {
	int32_t pid;
	struct my_thread_struct thread_struct;
};

static void print_my_page(const struct my_page* mp) {
	printk("\nPage:\n");
	printk("flags: %lu\n", mp->flags);
	printk("Virtual address: %ld, Page address: %p\n", mp->vm_address, mp->mapping);
}

static struct my_thread_struct get_my_thread_struct(struct task_struct* task) {
	struct thread_struct ts = task->thread;
	pr_info("task = %d\n", task);
	struct my_thread_struct mts = {
		.sp = ts.sp,
		.es = ts.es,
		.ds = ts.ds,
		.fsindex = ts.fsindex,
		.gsindex = ts.gsindex,
		.fsbase = ts.fsbase,
		.gsbase = ts.gsbase,
		.debugreg6 = ts.debugreg6,
		.ptrace_dr7 = ts.ptrace_dr7,
		.cr2 = ts.cr2,
		.trap_nr = ts.trap_nr,
		.error_code = ts.error_code,
		.io_bitmap_ptr = ts.io_bitmap_ptr,
		.iopl = ts.iopl,
		.io_bitmap_max = ts.io_bitmap_max
	};
	return mts;
}

static struct page* get_page_by_mm_and_address(struct mm_struct* mm, long address) {
	pgd_t* pgd;
	p4d_t* p4d;
	pud_t* pud;
	pmd_t* pmd;
	pte_t* pte;
	struct page* page;
	pgd = pgd_offset(mm, address);
	if (!pgd_present(*pgd)) return NULL;
	p4d = p4d_offset(pgd, address);
	if (!p4d_present(*p4d)) return NULL;
	pud = pud_offset(p4d, address);
	if (!pud_present(*pud)) return NULL;
	pmd = pmd_offset(pud, address);
	if (!pmd_present(*pmd)) return NULL;
	pte = pte_offset_map(pmd, address);
	if (!pte_present(*pte)) return NULL;
	page = pte_page(*pte);
	return page;
}

// This function will be called when we write IOCTL on the Device file
static long driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct my_page mp;
	struct page* page;
	struct task_struct* task;
	struct mm_struct* mm;
	struct vm_area_struct* vm;
	switch(cmd) {
		case WR_PID:
			if(copy_from_user(&pid, (int*) arg, sizeof(pid))) pr_err("Data write error!\n");
			pr_info("Pid = %d\n", pid);
			break;
		case RD_MY_PAGE:
			task = get_pid_task(find_get_pid(pid), PIDTYPE_PID);
			mm = task->mm;
			vm = mm->mmap;
			if (mm == NULL) {
				printk(KERN_INFO "Task struct hasn't mm\n");
			}
			long address = vm->vm_start;
			long end = vm->vm_end;
			while (address <= end) {
				page = get_page_by_mm_and_address(mm, address);
				if (page != NULL) {
					mp.flags = page->flags;
					mp.mapping = (void*)page->mapping;
					mp.vm_address = address;
					break;
				}
				address += PAGE_SIZE;
			}
			print_my_page(&mp);
			if(copy_to_user((struct my_page*) arg, &mp, sizeof(struct my_page))) {
				printk(KERN_INFO "Data read error!\n");
			}
			break;
		case RD_MY_THREAD_STRUCT:
			pr_info("RD_MY_THREAD_STRUCT ");
			pr_info("arg = %d\n", arg);
			struct thread_struct_request tsr;
			if(copy_from_user(&tsr, (int*) arg, sizeof(struct thread_struct_request))) 	pr_err("Data write error!\n");
			pr_info("Pid = %d\n", tsr.pid);
			struct task_struct* task = get_pid_task(find_get_pid(tsr.pid), PIDTYPE_PID);
			if (!task) {
				printk(KERN_INFO "PID not found!\n");
				break;
			}
			struct my_thread_struct mts = get_my_thread_struct(task);
			tsr.thread_struct = mts;	
			if(copy_to_user((struct thread_struct_request*) arg, &tsr, sizeof(struct thread_struct_request))) {
				printk(KERN_INFO "Data read error!\n");
			}
			break;
		default:
			pr_info("Command not found!");
			break;
	}
	return 0;
}

static struct file_operations fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = driver_ioctl,
};

static int __init ioctl_core_init(void) {
	printk(KERN_INFO "Core mode is started, hello, world!\n");
	int retval;
	retval = register_chrdev(MYMAJOR, "my_driver", &fops);
	if (0 == retval) {
		printk("my_driver device number Major:%d , Minor:%d\n", MYMAJOR, 0);
	} else if (retval > 0) {
		printk("my_driver device number Major:%d , Minor:%d\n", retval >> 20, retval & 0xffff);
	} else {
		printk("Couldn't register device number!\n");
		return -1;
	}
	return 0;
}

static void __exit ioctl_core_exit(void) {
	unregister_chrdev(MYMAJOR, "my_driver");
	printk(KERN_INFO "Core mode is finished, goodbye!\n");
}

module_init(ioctl_core_init);
module_exit(ioctl_core_exit);
