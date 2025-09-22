#include <stdint.h>

static void outb(uint16_t port, uint8_t value) {
	asm volatile ("outb %0,%1" : /* dest */ : "a" (value), "Nd" (port) : "memory");
}

static uint8_t inb(uint16_t port){
	uint8_t result = 0;
	asm volatile ("inb %1, %0" : "=a" (result) : "Nd" (port) : "memory");
	return result;
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	const char *p;
	uint16_t port = 0xE9;
	// uint8_t c = inb(port);
	// outb(port, c);
	for (p = "Poz2!\n"; *p; ++p)
		outb(0xE9, *p);

	for (;;)
		asm("hlt");
}
