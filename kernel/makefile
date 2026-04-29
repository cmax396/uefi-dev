CC     = clang
CFLAGS = -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
         -mno-red-zone -nostdlib -O2 -Wall -Wextra \
         -I../include

LD      = ld.lld
LDFLAGS = -T linker.ld --no-dynamic-linker -z noexecstack

KERNEL  = kernel.elf
SRCS    = src/kmain.c
OBJS    = $(SRCS:.c=.o)

.PHONY: clean

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(KERNEL)
