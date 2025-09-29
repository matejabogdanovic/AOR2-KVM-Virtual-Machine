#include <stdint.h>
#include "./api.c"

void citanje_shared(){
int fhandle = kvm_fopen("./files/file.txt", KVM_FILE_READ | KVM_FILE_WRITE);
	int cnt = 10;

	char buffer[256] = {0};
	int read = kvm_fread(fhandle, buffer, sizeof(char),	cnt);
	for (int i = 0; i < read; i++)
		{
			kvm_putc(buffer[i]);
		}


	kvm_putc('0'+kvm_fclose(fhandle));
	for (;;)
		asm("hlt");
}
void pisanje_shared(){
int fhandle = kvm_fopen("./files/file.txt", KVM_FILE_READ | KVM_FILE_WRITE);
	int cnt = 10;

	char buffer[256] = {0};
	int read = kvm_fread(fhandle, buffer, sizeof(char),	cnt);
	for (int i = 0; i < read; i++)
		{
			kvm_putc(buffer[i]);
		}

	kvm_fwrite(fhandle, buffer, sizeof(char), read);
	kvm_putc('0'+kvm_fclose(fhandle));
	
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	// citanje_shared();
	pisanje_shared();
	for (;;)
		asm("hlt");
}

