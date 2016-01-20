obj-m += submitjob.o

submitjob-y := queue.o sys_submitjob.o functionality.o

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

all: xhw3 submitjob

nlink.o:
	gcc -c -Wall -Werror nlink.c -o nlink.o
xhw3: xhw3.c nlink.o
	gcc -Wall -Werror -I/lib/modules/$(shell uname -r)/build/arch/x86/include xhw3.c nlink.o -o xhw3 -lssl

submitjob:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f xhw3
