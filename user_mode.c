#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <inttypes.h>
#include <linux/ptrace.h>

#define WR_PID _IOW('a','a', int32_t*)
#define RD_MY_PAGE _IOR('a','c', struct my_page*)
#define RD_MY_THREAD_STRUCT _IOWR('a','a', struct thread_struct_request*)

struct my_page {
	unsigned long flags;
	void* mapping;
	long vm_address;
};

struct my_seccomp_data {
	int nr;
	__u32 arch;
	__u64 instruction_pointer;
	__u64 args[6];
};

static void print_line(void) {
	printf("________________________\n");
}



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

static void print_my_thread_struct(struct my_thread_struct mts) {
	printf("thread_struct:\n");
	printf("    sp: %lu\n", mts.sp);
	printf("    es: %hu\n", mts.es);
	printf("    ds: %hu\n", mts.ds);
	printf("    fsindex: %hu\n", mts.fsindex);
	printf("    gsindex: %hu\n", mts.gsindex);
	printf("    fsbase: %lu\n", mts.fsbase);
	printf("    gsbase: %lu\n", mts.gsbase);
	printf("    debugreg6: %lu\n", mts.debugreg6);
	printf("    ptrace_dr7: %lu\n", mts.ptrace_dr7);
	printf("    cr2: %lu\n", mts.cr2);
	printf("    trap_nr: %lu\n", mts.trap_nr);
	printf("    error_code: %lu\n", mts.error_code);
	printf("    io_bitmap_ptr: %" PRIXPTR "\n", (uintptr_t)mts.io_bitmap_ptr);
	printf("    iopl: %lu\n", mts.iopl);
	printf("    io_bitmap_max: %u\n", mts.io_bitmap_max);
}

static int thread_struct_find(int32_t pid) {
	int fd = open("/dev/lab2_driver", O_WRONLY);
	if(fd < 0) {
		printf("Cannot open device file\n");
		return 0;
	}

    struct thread_struct_request tsr = {
        .pid = pid
    };

	ioctl(fd, RD_MY_THREAD_STRUCT, &tsr);
    printf("pid = %d\n", tsr.pid);
    print_my_thread_struct(tsr.thread_struct);

	close(fd);
	return 0;
}

static void print_page(const struct my_page* mp) {
	printf("\nPage:\n\n");
	printf("flags: %ld\n", mp->flags);
	printf("Virtual address: %ld\n", mp->vm_address);
	printf("Page address: %p\n", mp->mapping);
}

int main(int argc, char *argv[]) {
	int fd;
	if(argc < 2) {
		printf("The program needs an argument - a pid!\n");
		return -1;
	}
	int32_t pid = atoi(argv[1]);
	if (pid < 1) {
		printf("Pid can be only greater than 0\n");
		return -1;
	}
	printf("\nOpening a driver...\n");
	fd = open("/dev/lab2_driver", O_WRONLY);
	if(fd < 0) {
		printf("Cannot open device file:(\n");
		return 0;
	}
	struct my_page mp;
	// Writing a pid to driver
	ioctl(fd, WR_PID, (int32_t*) &pid);
	// Reading my_page from driver
	ioctl(fd, RD_MY_PAGE, &mp);
	print_line();
	print_page(&mp);
	thread_struct_find(pid);
	print_line();
	printf("Closing a driver...\n");
	close(fd);
}
