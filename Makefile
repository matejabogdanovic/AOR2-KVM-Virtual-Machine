
all: guest.img mini_hypervisor

mini_hypervisor: mini_hypervisor.c
	gcc -g mini_hypervisor.c -o mini_hypervisor

guest.img: guest.o
	ld -T guest.ld guest.o -o guest.img

guest.o: guest.c
	$(CC) -m64 -ffreestanding -fno-pic -c -o $@ $^

clean:
	rm -f mini_hypervisor guest.o guest.img