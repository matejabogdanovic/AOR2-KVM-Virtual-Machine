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

uint64_t GUEST_START_ADDR = 0x0; // Početna adresa za učitavanje gosta
#define ENABLE_LOG 0

#if ENABLE_LOG
  #define LOG(x) x;
#else
  #define LOG(x) do {} while(0);
#endif

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


#define IO_PORT 0xE9
int GUEST_IMG = FALSE;

size_t MEM_SIZE = FALSE; // 2 or 4 or 8 MB
int PAGE_SIZE = FALSE; // 2(MB) or 4(KB)
size_t page_tables_sz = 0;
static void setup_long_mode(struct vm *v, struct kvm_sregs *sregs)
{
	// Postavljanje 4 niva ugnjezdavanja.
	// Svaka tabela stranica ima 512 ulaza, a svaki ulaz je veličine 8B.
  // Odatle sledi da je veličina tabela stranica 4KB. Ove tabele moraju da budu poravnate na 4KB
	// jer su velicine 4KB.

	// PDE64_PRESENT - stranica ucitana u memoriju 
	// PDE64_RW - citanje i upis dozvoljeno
	// PDE64_USER - prava pristupa USER - vm ce moci da pristupa samo tim stranicama
	// a ne supervisor stranicama
	
	size_t page_sz = ((PAGE_SIZE==2)?(2*1024*1024):(4*1024));
	size_t page_cnt = MEM_SIZE/ page_sz;
	LOG(printf("page_cnt = %ld, ", page_cnt));



	// guest start adresa je prva slobodna adresa nakon sto smestimo tabele
	// rip ce pocinjati od 0 sto znaci da ce se slikati u ulaz 0
	// ulaz 0 ce pokazivati na GUEST_START_ADDR
	// tabele stranica su fizicki na pocetku ali su postavljene na kraj VAP
	// zato SP mora poceti od MEM_SIZE - sizeoftabele
	uint64_t pml4_addr = (MEM_SIZE-0x1000);
	uint64_t *pml4 = (void *)(v->mem + pml4_addr); 
	uint64_t pdpt_addr = (pml4_addr-0x1000);
	uint64_t *pdpt = (void *)(v->mem + pdpt_addr); //  pokazivac na memoriju gde ce biti tabela 3
	uint64_t pd_addr = (pdpt_addr-0x1000);
	uint64_t *pd = (void *)(v->mem + pd_addr); //  pokazivac na memoriju gde ce biti tabela 2
	pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr; // uvek 1 ulaz
	pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;
	uint64_t page = 0;
	if(PAGE_SIZE==4){
		size_t pt_cnt = (page_cnt+ 512 - 1) / 512; // 1, 2, 4
		LOG(printf("pt_cnt = %ld \n", pt_cnt));

		uint64_t pt_addr = (pd_addr-0x1000*pt_cnt);
		uint64_t *pt = (void *)(v->mem + pt_addr); //  pokazivac na memoriju gde ce biti tabela 1

		page_tables_sz = MEM_SIZE - pt_addr;
		LOG(printf("page_tables_sz = %#lx\n", page_tables_sz));

		for(int i = 0; i < pt_cnt; i++){
			pd[i] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pt_addr ;
			
			for(int j = 0; j < 512; j++){ 
				pt[j] = page | PDE64_PRESENT | PDE64_RW | PDE64_USER;
				page += page_sz;	
			}	
			pt_addr += 0x1000; // poravnato na zbog velicine tabela
			pt = (void *)(v->mem + pt_addr);
		}	
		
		
	}else{ // pt unused

		page_tables_sz = MEM_SIZE - pd_addr;
		LOG(printf("page_tables_sz = %#lx\n", page_tables_sz));
		LOG(printf("pd_addr = %#lx\n", pd_addr));

		
		for(int j = 0; j < page_cnt; j++){
			 
			pd[j] = page | PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS; 
			LOG(printf("%#lx\n", page));
			page += page_sz;	
		}	

		
		
	}

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

	if (vm_init(&v, MEM_SIZE)) {
		printf("Failed to init the VM\n");
		return 0;
	}

	// Dohvatanje specijalnih registara
	if (ioctl(v.vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		vm_destroy(&v);
		return 0;
	}
	
	setup_long_mode(&v, &sregs);
	// postavljanje sregs
  if (ioctl(v.vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		vm_destroy(&v);
		return 0;
	}
	// ucitavanje img
	if (load_guest_image(&v, argv[GUEST_IMG], GUEST_START_ADDR) < 0) {
		printf("Failed to load guest image\n");
		vm_destroy(&v);
		return 0;
	}
	// postavljanje registar
	memset(&regs, 0, sizeof(regs));
	regs.rflags = 0x2;
	
	// PC se preko pt[0] ulaza mapira na fizičku adresu GUEST_START_ADDR (0x0).
	// a na GUEST_START_ADDR je učitan gost program.
	regs.rip = 0x0; 

	regs.rsp = (MEM_SIZE-page_tables_sz); // SP raste nadole
	LOG(printf("rsp = %#llx\n", regs.rsp));
	 
	// strcpy(v.mem,"Poruka od hipervizora :)\n");

	if (ioctl(v.vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		return 0;
	}

	// hipervizor
	while(stop == 0) {
		ret = ioctl(v.vcpu_fd, KVM_RUN, 0);
		if (ret == -1) {
			printf("KVM_RUN failed\n");
			vm_destroy(&v);
			return 0;
		}

		switch (v.run->exit_reason) {
			case KVM_EXIT_IO:
				if (v.run->io.direction == KVM_EXIT_IO_OUT && v.run->io.port == IO_PORT) {
					char *p = (char *)v.run;
					printf("%c", *(p + v.run->io.data_offset));
				}else if(v.run->io.direction == KVM_EXIT_IO_IN && v.run->io.port == IO_PORT){
					char c = getchar();
					char *p = (char *)v.run;
					*(p + v.run->io.data_offset) = c;
				}else{
					printf("KVM_EXIT_IO unknown reason\n");
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
