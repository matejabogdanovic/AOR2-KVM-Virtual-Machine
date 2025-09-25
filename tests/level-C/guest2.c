#include <stdint.h>
#include "./api.c"

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	const char *p;
	
	kvm_getc(); 
	kvm_print("12345678");



	for (;;)
		asm("hlt");
}

