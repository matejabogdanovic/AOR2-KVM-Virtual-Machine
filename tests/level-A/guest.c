

#include <stdint.h>
uint16_t PORT_IO = 0xE9;

static void outb(uint8_t value, uint16_t port) {
	asm volatile ("outb %0,%1" : /* dest */ : "a" (value), "Nd" (port) : "memory");
}

static uint8_t inb(uint16_t port){
	uint8_t result = 0;
	asm volatile ("inb %1, %0" : "=a" (result) : "Nd" (port) : "memory");
	return result;
}

static void print(const char* p){
	for (; *p; ++p)
		outb(*p, PORT_IO);
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	const char *p;

	print("Upisi slovo: ");
	uint8_t c = inb(PORT_IO);
	print("Echo: ");
	outb(c, PORT_IO);
	c = inb(PORT_IO);
	outb(c, PORT_IO); // print '\n' from console



	for (;;)
		asm("hlt");
}

