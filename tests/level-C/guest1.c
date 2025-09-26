#include <stdint.h>
#include "./api.c"

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	const char *p;

	int fhandle = kvm_fopen("./files/file.txt", KVM_FILE_READ | KVM_FILE_WRITE);
	int fhandle2 = kvm_fopen("./files/file.txt", KVM_FILE_READ | KVM_FILE_WRITE);
	kvm_putc('0'+fhandle);
	kvm_putc('0'+fhandle2);
	char buffer[3] = {'e', 'e', 'e'};
	int ret = kvm_fread(fhandle, buffer, sizeof(char), 3);
	kvm_putc('0'+ret);

	for (int i = 0; i < 3; i++)
	{
		kvm_putc(buffer[i]++);
	}
	kvm_fwrite(fhandle, buffer, sizeof(char), 3);
	kvm_putc('0'+kvm_fseek(fhandle, 1, KVM_SEEK_CUR));
	kvm_putc('0'+kvm_fclose(fhandle));

	
	// kvm_print("Type 'm': ");
	// uint8_t c = 0;
	// while ((c = kvm_getc())!='m'){}

	// for (uint8_t* i = 0; *i != 0; ++i)
	// 	kvm_putc(*(i));
	
	for (;;)
		asm("hlt");
}

