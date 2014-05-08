ifeq ($(shell uname),Linux)
CC		= gcc
ASM		= nasm
LD		= ld
OS		= Linux
else
CC		= /opt/local/bin/i386-elf-gcc
ASM 	= /opt/local/bin/nasm
LD		= /opt/local/bin/i386-elf-ld
OS		= Darwin
endif

CSTRICT	= -Werror=return-type -Werror=uninitialized
CFLAGS	= -m32 -c $(CSTRICT) -fno-stack-protector -fno-builtin -I./ -DDEBUG_FS
ASFLAGS	= -f elf32
LDFILE	= -m elf_i386 -T link.ld 
LDFLAGS	= $(LDFILE)
TARGET	= kernel
OBJS	= boot.o \
		  kernel.o \
		  tty.o \
		  klib.o\
		  int.o\
		  interrupt.o\
		  keyboard.o\
		  list.o\
		  dsr.o\
		  mm.o\
		  timer.o\
		  ps.o\
		  lock.o\
		  vfs.o\
		  ext2.o\
          hdd.o\
          block.o\
		  mount.o\
		  namespace.o

all: kernel


kernel: $(OBJS)
	$(LD) $(LDFLAGS) -e 0x100010 -o $(TARGET) $(OBJS)

boot.o: boot/kernel.asm
	$(ASM) $(ASFLAGS) boot/kernel.asm -o boot.o

int.o: int/int.S
	$(CC) $(CFLAGS) int/int.S -o int.o

interrupt.o: int/int.c int/int.h
	$(CC) $(CFLAGS) int/int.c -o interrupt.o 

kernel.o: boot/kernel.c
	$(CC) $(CFLAGS) boot/kernel.c -o kernel.o

tty.o: drivers/tty.c drivers/tty.h
	$(CC) $(CFLAGS) drivers/tty.c -o tty.o

keyboard.o: drivers/keyboard.c drivers/keyboard.h
	$(CC) $(CFLAGS) drivers/keyboard.c -o keyboard.o

klib.o: lib/klib.c lib/klib.h drivers/tty.h
	$(CC) $(CFLAGS) lib/klib.c -o klib.o

list.o: lib/list.c lib/list.h
	$(CC) $(CFLAGS) lib/list.c -o list.o

dsr.o: int/dsr.c int/dsr.h
	$(CC) $(CFLAGS) int/dsr.c -o dsr.o

mm.o: mm/mm.c mm/mm.h boot/multiboot.h
	$(CC) $(CFLAGS) -o mm.o mm/mm.c

timer.o: int/timer.c int/timer.h
	$(CC) $(CFLAGS) -o timer.o int/timer.c

ps.o: ps/ps.c ps/ps.h
	$(CC) $(CFLAGS) -o ps.o ps/ps.c

lock.o: ps/lock.c ps/lock.h
	$(CC) $(CFLAGS) -o lock.o ps/lock.c


vfs.o: fs/vfs.c fs/vfs.h
	$(CC) $(CFLAGS) -o vfs.o fs/vfs.c

ext2.o: fs/ext2.c fs/ext2.h
	$(CC) $(CFLAGS) -o ext2.o fs/ext2.c

block.o: drivers/block.h drivers/block.c
	$(CC) $(CFLAGS) -o block.o drivers/block.c

hdd.o: drivers/hdd.h drivers/hdd.c
	$(CC) $(CFLAGS) -o hdd.o drivers/hdd.c

mount.o: fs/mount.c fs/mount.h
	$(CC) $(CFLAGS) -o mount.o fs/mount.c

namespace.o: fs/namespace.c fs/namespace.h
	$(CC) $(CFLAGS) -o namespace.o fs/namespace.c

user: user/run.h user/run.c
ifeq ($(OS),Linux)
	cd user && make -f Makefile run
	-mkdir mnt
	sudo losetup -o 1048576 /dev/loop0 rootfs.img
	sudo mount /dev/loop0 mnt
	sudo cp user/run mnt/bin/
	ping -c 5 127.0.0.1 > /dev/null
	sudo umount mnt
	sudo losetup -d /dev/loop0
	rm -rf mnt
else
	cd user && make -f Makefile run
endif

clean:
	find . -name "*.o" -exec rm -f {} \;
	-rm $(TARGET)
	-rm user/run
