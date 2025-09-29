#include <stdint.h>
#include "./api.c"

void citanje(const char* path){
	int fhandle = kvm_fopen(path, KVM_FILE_READ );
	int cnt = 256;

	char buffer[256] = {0};
	int read = kvm_fread(fhandle, buffer, sizeof(char),	cnt);
	for (int i = 0; i < read; i++)
		{
			kvm_putc(buffer[i]);
		}
	kvm_putc('0'+kvm_fclose(fhandle));
}
void pisanje(const char* path){
	int fhandle = kvm_fopen(path,  KVM_FILE_WRITE);
	int cnt = 10;
	kvm_fwrite(fhandle, "Samo upis!", sizeof(char),cnt);
	kvm_putc('0'+kvm_fclose(fhandle));
}
void dodavanje_na_kraj_citanje(const char* path){
	int fhandle = kvm_fopen(path,  KVM_FILE_WRITE|KVM_FILE_READ);
	int cnt = 7;
	kvm_fseek(fhandle, 0, KVM_SEEK_END);
	kvm_fwrite(fhandle, "Append!", sizeof(char),cnt);

	kvm_fseek(fhandle, 0, KVM_SEEK_SET);
	char buffer[256] = {0};
	int read = kvm_fread(fhandle, buffer, sizeof(char),	256);
	for (int i = 0; i < read; i++)
		{
			kvm_putc(buffer[i]);
		}
	kvm_putc('0'+kvm_fclose(fhandle));
}

void citanje_pisanje(const char* path){
	int fhandle = kvm_fopen(path, KVM_FILE_READ | KVM_FILE_WRITE);
	int cnt = 256;

	char buffer[256] = {0};
	int read = kvm_fread(fhandle, buffer, sizeof(char),	256);
	for (int i = 0; i < read; i++)
		{
			kvm_putc(buffer[i]);
		}

	kvm_fwrite(fhandle, "citanje_pisanje", sizeof(char), 15);
	kvm_putc('0'+kvm_fclose(fhandle));
	
}
void pisanje_citanje(const char* path){

	int fhandle = kvm_fopen(path, KVM_FILE_READ | KVM_FILE_WRITE);
	int cnt = 8;
	kvm_fwrite(fhandle, "Pozdrav!", sizeof(char),cnt);
	
	char buffer[256] = {0};
	kvm_fseek(fhandle, 0, KVM_SEEK_SET);
	int read = kvm_fread(fhandle, buffer, sizeof(char),	cnt);
	for (int i = 0; i < read; i++)
		{
			kvm_putc(buffer[i]);
		}

	
	kvm_putc('0'+kvm_fclose(fhandle));
	
}

void pisanje_nema_privilegije(const char* path){
	int fhandle = kvm_fopen(path, KVM_FILE_READ);
	int cnt = 10;

	int written = kvm_fwrite(fhandle, "Tekst", sizeof(char), 5);
	

	kvm_putc('0'+written);

	kvm_putc('0'+kvm_fclose(fhandle));
	
}
void citanje_nema_privilegije(const char* path){
	int fhandle = kvm_fopen(path, KVM_FILE_WRITE);
	int cnt = 10;	
	char buffer[256] = {0};
	int read = kvm_fread(fhandle, buffer, sizeof(char),	cnt);
	kvm_putc('0'+read);
	for (int i = 0; i < read; i++)
	{
			kvm_putc(buffer[i]);
	}

	// kvm_fwrite(fhandle, "Tekst", sizeof(char), 5);

	kvm_putc('0'+kvm_fclose(fhandle));
	
}




void maintest(){
	// kvm_getc();
	int fhandle = kvm_fopen("./files/file2.txt", KVM_FILE_WRITE | KVM_FILE_READ);
	int cnt = 256;

	char buffer[256] = {0};
	int read = kvm_fread(fhandle, buffer, sizeof(char),	cnt);
	for (int i = 0; i < read; i++){
			kvm_putc(buffer[i]);
		}


	
	kvm_fwrite(fhandle, " Dodajem ovo.", sizeof(char),12);
	kvm_putc('0'+kvm_fclose(fhandle));
	citanje_pisanje("./files/lokalan_fajl");
}
void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	// dodavanje_na_kraj_citanje("./files/file2.txt");

	maintest();
	
	for (;;)
		asm("hlt");
}

