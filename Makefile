obj-m += core_mode.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	-rm -f user_mode
test:
	sudo dmesg -C
	sudo insmod core_mode.ko
usermode:
	gcc -pedantic-errors -Wall -Werror -g3 -O0 --std=c99 -fsanitize=address,undefined,leak ./user_mode.c -o user_mode
