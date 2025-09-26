#include <stdint.h>

uint16_t PORT_IO = 0xE9;
uint16_t PORT_FILE = 0x0278;

static void outl(uint32_t value, uint16_t port)
{
  asm volatile("outl %0,%1" : /* dest */ : "a" (value), "Nd" (port) : "memory");
}

static uint32_t inl(uint16_t port)
{
  uint32_t result = 0;
  asm volatile("inl %1,%0" : "=a" (result) : "Nd" (port) : "memory");
  return result;
}

static void outb(uint8_t value, uint16_t port) {
	asm volatile ("outb %0,%1" : /* dest */ : "a" (value), "Nd" (port) : "memory");
}

static uint8_t inb(uint16_t port){
	uint8_t result = 0;
	asm volatile ("inb %1, %0" : "=a" (result) : "Nd" (port) : "memory");
	return result;
}
// IO
void kvm_print(const char* p){
	for (; *p; ++p)
		outb(*p, PORT_IO);
}

char kvm_getc(){
  return inb(PORT_IO);
}

void kvm_putc(char c){
  return outb(c, PORT_IO);
}

// FILES
#define STATUS_INVALID 1
#define STATUS_VALID 0
#define EOS 0
#define FOPEN 1
#define FOPEN_MAX_PATHSIZE 256
#define FREAD 2
#define FWRITE 3
#define FSEEK 4
#define FCLOSE 5

#define KVM_FILE_READ 1 
#define KVM_FILE_WRITE 2 
#define KVM_FILE_RW 3 
// @returns 0 if ok
int kvm_fopen(const char* fname, int access){
	if(access != KVM_FILE_READ && access != KVM_FILE_WRITE && access != KVM_FILE_RW)
		return -1;
	if(sizeof(fname) > FOPEN_MAX_PATHSIZE)
		return -1;

	outb(FOPEN, PORT_FILE); // 1 je fopen

	// prosledi putanju
	for (; *fname; ++fname)
		outb(*fname, PORT_FILE);
	// kraj putanje
	outb(EOS, PORT_FILE);
	// prava pristupa
	
	outb(access, PORT_FILE);

	// dohvati fhandle
	return inl(PORT_FILE); 
}

int kvm_fread(int f, void* buffer, unsigned long size, unsigned long len){
	outb(FREAD, PORT_FILE); // 2 je fread
	outl(f, PORT_FILE); // prosledi fhandle
	if(inb(PORT_FILE) == STATUS_INVALID)return -1; // status da li smem da radim operaciju

	uint8_t c = 0; 
	uint8_t status = 0; 
	uint8_t *dst = (uint8_t*)buffer;
	unsigned long read = 0;
	for ( ;read < len*size; read++){
		c = inb(PORT_FILE); // probaj da procitas podatak
		status = inb(PORT_FILE); // dohvati status procitanog podatka
		if(status == STATUS_INVALID){ // nema vise podataka
			outb(EOS, PORT_FILE);
			return read;
		}
		dst[read] = c;
		
	}
	outb(EOS, PORT_FILE);
	return read;
}



int kvm_fwrite(int f, void* buffer, unsigned long size, unsigned long len){
	outb(FWRITE, PORT_FILE); // 3 je fwrite
	outl(f, PORT_FILE); // prosledi fhandle
	if(inb(PORT_FILE) == STATUS_INVALID)return -1; // status da li smem da radim operaciju

	uint8_t status = 0; 
	uint8_t *src = (uint8_t*)buffer;
	unsigned long written = 0;
	for (; written < len*size; written++){

		outb(src[written], PORT_FILE); // probaj da upises podatak
		status = inb(PORT_FILE); // dohvati status upisanog podatka
		if(status == STATUS_INVALID){ // ne moze da se upise
			inb(PORT_FILE);
			return written;
		}
		
	}
	inb(PORT_FILE);
	return written;
}
#define KVM_SEEK_END 0 // It denotes the end of the file.
#define KVM_SEEK_SET 1 // It denotes starting of the file.
#define KVM_SEEK_CUR 2 // It denotes the file pointer's current position. 
// It returns zero if successful, or else it returns a non-zero value.
int kvm_fseek(int f,long int offset, int position){
	if(position != KVM_SEEK_END && position != KVM_SEEK_SET && position != KVM_SEEK_CUR)return -1;

	outb(FSEEK, PORT_FILE); // 4 je fseek
	outl(f, PORT_FILE); // prosledi fhandle
	if(inb(PORT_FILE) == STATUS_INVALID)return -1; // status da li smem da radim operaciju

	outb(offset, PORT_FILE);
	outb(position, PORT_FILE);


	return inb(PORT_FILE);
}

// It returns zero if successful, or else it returns a non-zero value.
int kvm_fclose(int f){
	outb(FCLOSE, PORT_FILE); // 5 je fclose
	outl(f, PORT_FILE); // prosledi fhandle
	return inb(PORT_FILE);
}