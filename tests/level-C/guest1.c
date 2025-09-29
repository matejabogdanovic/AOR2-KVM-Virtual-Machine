#include <stdint.h>
#include "./api.c"

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {

	int fhandle = kvm_fopen("./files/file2.txt", KVM_FILE_WRITE | KVM_FILE_READ);
	int cnt = 256;

	char buffer[256] = {0};
	int read = kvm_fread(fhandle, buffer, sizeof(char),	cnt);
	for (int i = 0; i < read; i++){
			kvm_putc(buffer[i]);
		}

	kvm_fwrite(fhandle, " Dodajem ovo.", sizeof(char),12);
	kvm_putc('0'+kvm_fclose(fhandle));
	for (;;)
		asm("hlt");
}

