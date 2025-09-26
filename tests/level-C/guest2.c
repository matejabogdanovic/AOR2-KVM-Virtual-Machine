#include <stdint.h>
#include "./api.c"

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	int fhandle = kvm_fopen("lokal.txt", KVM_FILE_WRITE);
	// int fhandle2 = kvm_fopen("./files/file.txt", KVM_FILE_READ | KVM_FILE_WRITE);
	kvm_fclose(fhandle);
	// kvm_fclose(fhandle2);
	// fhandle = kvm_fopen("./files/file.txt", KVM_FILE_READ | KVM_FILE_WRITE);
	
	kvm_putc('0'+kvm_fclose(fhandle));
	for (;;)
		asm("hlt");
}

