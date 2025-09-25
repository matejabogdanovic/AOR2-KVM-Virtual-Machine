#include <stdint.h>
#include "./api.c"

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	const char *p;

	int fhandle = kvm_fopen("file", "rwe");
	kvm_putc('0'+fhandle);
	char buffer[3] = {'e', 'e', 'e'};
	int ret = kvm_fread(fhandle, buffer, sizeof(char), 3);
	kvm_putc('0'+ret);
	kvm_putc('\n');
	for (int i = 0; i < 3; i++)
	{
		kvm_putc(buffer[i]);
	}
	
	// kvm_print("Type 'm': ");
	// uint8_t c = 0;
	// while ((c = kvm_getc())!='m'){}

	// for (uint8_t* i = 0; *i != 0; ++i)
	// 	kvm_putc(*(i));
	
	for (;;)
		asm("hlt");
}

