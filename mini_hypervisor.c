// Prevođenje:
//    make
// Pokretanje:
//    ./kvm_zadatak3 guest.img
//
// Koristan link: https://www.kernel.org/doc/html/latest/virt/kvm/api.html
//                https://docs.amd.com/v/u/en-US/24593_3.43
//
// Zadatak: Omogućiti ispravno izvršavanje gost C programa. Potrebno je pokrenuti gosta u long modu.
//          Podržati stranice veličine 4KB i 2MB.
//
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>


#define TRUE 1
#define FALSE 0

#define GUEST_START_ADDR 0x8000 // Početna adresa za učitavanje gosta

// PDE bitovi
#define PDE64_PRESENT (1u << 0)
#define PDE64_RW (1u << 1)
#define PDE64_USER (1u << 2)
#define PDE64_PS (1u << 7)

// CR4 i CR0
#define CR0_PE (1u << 0)
#define CR0_PG (1u << 31)
#define CR4_PAE (1u << 5)

#define EFER_LME (1u << 8)
#define EFER_LMA (1u << 10)

struct vm {
	int kvm_fd;
	int vm_fd;
	int vcpu_fd;
	char *mem;
	size_t mem_size;
	struct kvm_run *run;
	int run_mmap_size;
};

int vm_init(struct vm *v, size_t mem_size)
{
	struct kvm_userspace_memory_region region;	

	memset(v, 0, sizeof(*v));
	v->kvm_fd = v->vm_fd = v->vcpu_fd = -1;
	v->mem = MAP_FAILED;
	v->run = MAP_FAILED;
	v->run_mmap_size = 0;
	v->mem_size = mem_size;

  // Sistemski poziv za otvaranje /dev/kvm
  // Povratna vrednost je deskriptor fajla
	v->kvm_fd = open("/dev/kvm", O_RDWR);
	if (v->kvm_fd < 0) {
		perror("open /dev/kvm");
		return -1;
	}
	// provera kvm api verzije - ne mora
    int api = ioctl(v->kvm_fd, KVM_GET_API_VERSION, 0);
    if (api != KVM_API_VERSION) {
        printf("KVM API mismatch: kernel=%d headers=%d\n", api, KVM_API_VERSION);
        return -1;
    }
	// KVM pruža API preko kog može da se komunicira sa njim
  // Komunikacija se vrši preko Input/Outpu sistemskih poziva
  // int ioctl(int fd, unsigned long request, ...);
  //    fd      - fajl deskriptor
  //    request - zahtev
  //    ...     - parametar koji zavisi od zahteva
  // 
  // KVM_CREATE_VM - kreira virtuelnu mašinu bez virtuelnog(ih) procesora i memorije
	v->vm_fd = ioctl(v->kvm_fd, KVM_CREATE_VM, 0);
	if (v->vm_fd < 0) {
		perror("KVM_CREATE_VM");
		return -1;
	}
	// alocira/kreira memoriju za nasu vm
	// MAP_ANONYMOUS - nema fajl vec u memory i inic na 0
	// MAP_SHARED - vise procesa pristupa delu memorije (hipervizor + vm)
	v->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (v->mem == MAP_FAILED) {
		perror("mmap mem");
		return -1;
	}
  // Podesavanje regiona memorije koji će se koristiti za VM
  // slot            - broj mapiranja. Definisanje novog regiona sa istim slot brojem će zameniti ovo mapiranje.
  // guest_phys_addr - Fizicka adresa kako je gost vidi.
  // memory_size     - velicina regiona.
  // userspace_addr  - početna adresa memorije.
	region.slot = 0;
	region.flags = 0;
	region.guest_phys_addr = 0;
	region.memory_size = v->mem_size;
	region.userspace_addr = (uintptr_t)v->mem;
    if (ioctl(v->vm_fd, KVM_SET_USER_MEMORY_REGION, &region) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
        return -1;
	}
	// Kreiranje virtuelnog CPU
  // Parametar: vCPU ID
	v->vcpu_fd = ioctl(v->vm_fd, KVM_CREATE_VCPU, 0);
    if (v->vcpu_fd < 0) {
		perror("KVM_CREATE_VCPU");
        return -1;
	}
  // Dohvatanje veličine kvm_run strukture
  // Parametar: /
	v->run_mmap_size = ioctl(v->kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (v->run_mmap_size <= 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
		return -1;
	}
	// Mapirati kvm_run strukturu na koju pokazuje lokalna promenljiva kvm_run
	v->run = mmap(NULL, v->run_mmap_size, PROT_READ | PROT_WRITE,
			     MAP_SHARED, v->vcpu_fd, 0);
	if (v->run == MAP_FAILED) {
		perror("mmap kvm_run");
		return -1;
	}

	return 0;
}

void vm_destroy(struct vm *v) {
	if (v->run && v->run != MAP_FAILED) {
		munmap(v->run, (size_t)v->run_mmap_size);
		v->run = MAP_FAILED;
	}

	if(v->mem && v->mem != MAP_FAILED) {
		munmap(v->mem, v->mem_size);
		v->mem = MAP_FAILED;
	}

	if (v->vcpu_fd >= 0) {
		close(v->vcpu_fd);
		v->vcpu_fd = -1;
	}

	if (v->vm_fd >= 0) {
		close(v->vm_fd);
		v->vm_fd = -1;
	}

	if (v->kvm_fd >= 0) {
		close(v->kvm_fd);
		v->kvm_fd = -1;
	}
}

static void setup_segments_64(struct kvm_sregs *sregs)
{
	// .selector = 0x8,
	struct kvm_segment code = {
		.base = 0,
		.limit = 0xffffffff,
		.present = 1, // Prisutan ili učitan u memoriji
		.type = 11, // Code: execute, read, accessed
		.dpl = 0, // Descriptor Privilage Level: 0 (0, 1, 2, 3)
		.db = 0, // Default size - ima vrednost 0 u long modu
		.s = 1, // Code/data tip segmenta
		.l = 1, // Long mode - 1
		.g = 1, // 4KB granularnost
	};
	struct kvm_segment data = code;
	data.type = 3; // Data: read, write, accessed
	data.l = 0;
	// data.selector = 0x10; // Data segment selector

	sregs->cs = code;
	sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = data;
}

// Omogucavanje long moda.
// Vise od long modu mozete prociati o stranicenju u glavi 5:
// https://docs.amd.com/v/u/en-US/24593_3.43
// Pogledati figuru 5.1 na stranici 128.

// za 4 nivoa ugnjezdavanja
// VA : [sign extend | Page-Map Level-4 Offset | Page-Directory Pointer Offset | Page-Directory Offset | Page-Table Offset | Physical-Page-Offset]
// VA : [...|	PML4(9b) |  PDPO(9b)  |  PDO(9b)  |  PTO(9b)  |  OFFSET(12b) ]
//					|9b		 	       ...
//					|			 				 ...													4KB page size
//    			>[     ]	 [     ]   [     ]	 [     ]   [     ] <- 12b offset unutar stranice
//    			 [ ... ]	 [ ... ]   [ ... ]   [ ... ]	 [ ... ]
//				-> [     ]52>[     ]52>[     ]52>[     ]52>[     ]
// CR3 : [  32b PLM4 start address | 000]
static void setup_long_mode(struct vm *v, struct kvm_sregs *sregs)
{
	// Postavljanje 4 niva ugnjezdavanja.
	// Svaka tabela stranica ima 512 ulaza, a svaki ulaz je veličine 8B.
  // Odatle sledi da je veličina tabela stranica 4KB. Ove tabele moraju da budu poravnate na 4KB
	// jer su velicine 4KB.

	// postavljamo tabelu u memoriju
	uint64_t page = 0;
	uint64_t pml4_addr = 0x1000; // Adrese su proizvoljne. Poravnato na 12 zbog kontrolnih bita.
	uint64_t *pml4 = (void *)(v->mem + pml4_addr); // pokazivac na memoriju gde ce biti tabela 4

	uint64_t pdpt_addr = 0x2000;
	uint64_t *pdpt = (void *)(v->mem + pdpt_addr); //  pokazivac na memoriju gde ce biti tabela 3

	uint64_t pd_addr = 0x3000;
	uint64_t *pd = (void *)(v->mem + pd_addr); //  pokazivac na memoriju gde ce biti tabela 2

	uint64_t pt_addr = 0x4000;
	uint64_t *pt = (void *)(v->mem + pt_addr); //  pokazivac na memoriju gde ce biti tabela 1

	// PDE64_PRESENT - stranica ucitana u memoriju 
	// PDE64_RW - citanje i upis dozvoljeno
	// PDE64_USER - prava pristupa USER - vm ce moci da pristupa samo tim stranicama
	// a ne supervisor stranicama

	// Ulancavanje tabela
	pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr;
	pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;
	// 2MB page size
	// pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;

	// 4KB page size
	// -----------------------------------------------------
	pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pt_addr;
	// PC vrednost se mapira na ovu stranicu.
	pt[0] = GUEST_START_ADDR | PDE64_PRESENT | PDE64_RW | PDE64_USER;
	// SP vrednost se mapira na ovu stranicu. Vrednost 0x6000 je proizvoljno tu postavljena.
  pt[511] = 0x6000 | PDE64_PRESENT | PDE64_RW | PDE64_USER;
	
	// FOR petlja služi tome da mapiramo celu memoriju sa stranicama 4KB.
	// Zašto je uslov i < 512? Odgovor: jer je memorija veličine 2MB.
	// 2MB/4KB = 512
	// page = 0;
	// for(int i = 1; i < 512; i++) {
	// 	pt[i] = page | PDE64_PRESENT | PDE64_RW | PDE64_USER;
	// 	page += 0x1000;
	// }

	// -----------------------------------------------------

	// Registar koji ukazuje na PML4 tabelu stranica. Odavde kreće mapiranje VA u PA.
	// ili na PML5 base address when 5-Level paging is enabled (CR4[LA57]=1)
	sregs->cr3  = pml4_addr; 
	sregs->cr4  = CR4_PAE; // "Physical Address Extension" mora biti 1 za long mode.
	sregs->cr0  = CR0_PE | CR0_PG; // Postavljanje "Protected Mode" i "Paging" 
	sregs->efer = EFER_LME | EFER_LMA; // Postavljanje  "Long Mode Active" i "Long Mode Enable"

	// Inicijalizacija segmenata za 64-bitni mod rada.
	setup_segments_64(sregs);
}

int load_guest_image(struct vm *v, const char *image_path, uint64_t load_addr) {
	FILE *f = fopen(image_path, "rb");

	if (!f) {
		perror("Failed to open guest image");
		return -1;
	}
	// procitaj velicinu fajla
	if (fseek(f, 0, SEEK_END) < 0) {
		perror("Failed to seek to end of guest image");
		fclose(f);
		return -1;
	}

	long fsz = ftell(f);
	if (fsz < 0) {
		perror("Failed to get size of guest image");
		fclose(f);
		return -1;
	}
	rewind(f);
	// provera velicine fajla da li fajl staje u memoriju
	if((uint64_t)fsz > v->mem_size - load_addr) {
		printf("Guest image is too large for the VM memory\n");
		fclose(f);
		return -1;
	}
	// ucitavanje na adresu load_addr
	if (fread((uint8_t*)v->mem + load_addr, 1, (size_t)fsz, f) != (size_t)fsz) {
		perror("Failed to read guest image");
		fclose(f);
		return -1;
	}
	fclose(f);

	return 0;
}


// ./mini_hypervisor --memory or -m 2or4or8 --page or -p 4or2 --guest guest.img
// Обезбедити следеће основне особине хипервизора:
// • Величина физичке меморије госта је 2MB, 4МB или 8MB. Одговарајућа величина се задаје као
// параметар командне линије хипервизора преко опције -m или --memory.
// • Виртуелна машина (ВМ) раде у 64-битном моду (long mode).
// • Величина странице је 4KB или 2MB. Одговарајућа величина се задаје као параметар командне
// линије хипервизора преко опције -p или --page.
// • ВМ са само једним виртуелним процесором.
// • Подржава серијски испис и читање на IO порт 0xE9. Величина података који може да се
// пише/прочита на/са порт је 1 бајт.
// • Подржава само ВМ које завршавају извршавање инструкцијом hlt.
// • Учитавање и покретање госта који је дат као параметар командне линије хипервизора преко
// опције -g или --guest.

// #define MEM_SIZE (2u * 1024u * 1024u) // Veličina memorije će biti 2MB


int GUEST_IMG = FALSE;
size_t MEM_SIZE = FALSE; // 2 4 8 MB
int PAGE_SIZE = FALSE; // 2MB or 4KB
int parse_arguments(int argc, char *argv[]){
	for(int i = 1; i < argc; i++){
		char* param = argv[i];
		if(!strcmp(param, "--memory") || !strcmp(param, "-m")){
			if(MEM_SIZE){
				printf("invalid arguments: memory size already defined\n");
				return -1;
			}
			if(argc <= i+1){
				printf("not enough arguments\n");
				return -1;
			}
			int mem_sz_in_mb = atoi(argv[++i]);
			if(mem_sz_in_mb != 2 && mem_sz_in_mb != 4 && mem_sz_in_mb != 8){
				printf("invalid arguments: memory size must be 2, 4 or 8\n");
				return -1;
			}
			MEM_SIZE = (mem_sz_in_mb * 1024 * 1024);

		}else if(!strcmp(param, "--page") || !strcmp(param, "-p")){
			if(PAGE_SIZE){
				printf("invalid arguments: page size already defined\n");
				return -1;
			}
			if(argc <= i+1){
				printf("not enough arguments\n");
				return -1;
			}
			PAGE_SIZE = atoi(argv[++i]);
			if(PAGE_SIZE != 2 && PAGE_SIZE != 4 ){
				printf("invalid arguments: page size must be 2(MB) or 4(KB)\n");
				return -1;
			}
			
		}else if(!strcmp(param, "--guest") || !strcmp(param, "-g")){
			if(GUEST_IMG){
				printf("invalid arguments: guest image already defined\n");
				return -1;
			}
			if(argc <= i+1){
				printf("not enough arguments\n");
				return -1;
			}
			GUEST_IMG = ++i;
		}
		else{
			
			printf("unknown argument: %s\n", param);
			return -1;
	
		}
	}
	if(MEM_SIZE==FALSE || PAGE_SIZE==FALSE || GUEST_IMG==FALSE){
		printf("not enough arguments\n");
		return -1;
	}
	printf("MEM_SIZE = %lx \n", MEM_SIZE);
	printf("PAGE_SIZE = %d \n", PAGE_SIZE);
	printf("GUEST_IMAGE = %s \n", argv[GUEST_IMG]);
}

int main(int argc, char *argv[])
{
	if(parse_arguments(argc, argv) < 0){
		return -1;
	}

	struct vm v;
	struct kvm_sregs sregs; // specijalni registri
	struct kvm_regs regs; // registri
	int stop = 0;
	int ret = 0;
	FILE* img;

	// if (argc != 2) {
  //   	printf("The program requests an image to run: %s <guest-image>\n", argv[0]);
  //   	return 1;
  // 	}

	if (vm_init(&v, MEM_SIZE)) {
		printf("Failed to init the VM\n");
		return 1;
	}

	// Dohvatanje specijalnih registara
	if (ioctl(v.vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		vm_destroy(&v);
		return 1;
	}

	setup_long_mode(&v, &sregs);
	// postavljanje sregs
  if (ioctl(v.vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		vm_destroy(&v);
		return 1;
	}
	// ucitavanje img
	if (load_guest_image(&v, argv[GUEST_IMG], GUEST_START_ADDR) < 0) {
		printf("Failed to load guest image\n");
		vm_destroy(&v);
		return 1;
	}
	// postavljanje registar
	memset(&regs, 0, sizeof(regs));
	regs.rflags = 0x2;
	
	// PC se preko pt[0] ulaza mapira na fizičku adresu GUEST_START_ADDR (0x8000).
	// a na GUEST_START_ADDR je učitan gost program.
	regs.rip = 0; 
	regs.rsp = 2 << 20; // SP raste nadole, slika se u poslednji ulaz jer je adresa iza poslednje

	if (ioctl(v.vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		return 1;
	}

	// hipervizor
	while(stop == 0) {
		ret = ioctl(v.vcpu_fd, KVM_RUN, 0);
		if (ret == -1) {
			printf("KVM_RUN failed\n");
			vm_destroy(&v);
			return 1;
		}

		switch (v.run->exit_reason) {
			case KVM_EXIT_IO:
				if (v.run->io.direction == KVM_EXIT_IO_OUT && v.run->io.port == 0xE9) {
					char *p = (char *)v.run;
					printf("%c", *(p + v.run->io.data_offset));
				}
				continue;
			case KVM_EXIT_HLT:
				printf("KVM_EXIT_HLT\n");
				stop = 1;
				break;
			case KVM_EXIT_SHUTDOWN:
				printf("Shutdown\n");
				stop = 1;
				break;
			default:
				printf("Default - exit reason: %d\n", v.run->exit_reason);
				break;
    	}
  	}

	vm_destroy(&v);
}
