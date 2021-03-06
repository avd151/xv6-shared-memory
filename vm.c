#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "shm.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

struct sharedMemRegion allSharedMemRegions [SHARED_MEM_REGIONS];

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;
	
  if(newsz >= HEAPLIMIT) //earlier - newsz >= KERNBASE
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;
  
  if(oldsz >= KERNBASE){
	  oldsz = HEAPLIMIT; //max heap size = till HEAPLIMIT
  }

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

//Shared Memory Functions 

void sharedMemoryInit(void){
	//Iniitialize shared memory region 
	//todo: Shared memory pages - initialised in proc.
	int i = 0;
	for(i = 0; i < SHARED_MEM_REGIONS; i++){
		allSharedMemRegions[i].key = -1;
		allSharedMemRegions[i].valid = 0;
		allSharedMemRegions[i].shmid = -1;
		for(int j = 0; j < SHARED_MEM_REGIONS; j++){
			//check: Initialize physical address to 0
			allSharedMemRegions[i].physicalAddress[j] = (void*)0;
		}
	}
}

//shmget
int shmgetUtil(int key, int size, int shmflag)
{
	//cprintf("In shmget util\n");
	int notused = -1, notAllocFlag = 0;
	int i;
	int shmid;
	struct proc *currProc = myproc();
	//no. of pages to allocate
	int pagesToAlloc = size/PGSIZE + (size%PGSIZE != 0);
	if(pagesToAlloc < 0 || pagesToAlloc > SHARED_MEM_REGIONS){
		return -1;
	}
	//Find permissions from shmflag
	//xv6 = single user => lower 7 bits considered
	int lowerBits = shmflag & 7;
	int currPermission = -1;
	if(lowerBits == (int)RW_SHM){
		currPermission = RW_SHM;
		shmflag ^= RW_SHM;
	}
	else if(lowerBits == (int)READ_SHM){
		currPermission = READ_SHM;
		shmflag ^= READ_SHM;
	}
	else if((shmflag == 0) && (key == IPC_PRIVATE)){
			return -1;
	}
	if(size < 0 || size > KERNBASE){
		return -1;
	}
	//Find if there is already valid shared memory region associated with given key
	for(i = 0; i < SHARED_MEM_REGIONS; i++){
		//valid and already allocated
		if(allSharedMemRegions[i].key == key && allSharedMemRegions[i].valid){
		//If shared memory region exists but size != current asked size
			if(allSharedMemRegions[i].size != pagesToAlloc){
				//cprintf("Error: size of shared mem regions don't match\n");
				return -1; 
			}
			//check permissions 
			int regionPerm = allSharedMemRegions[i].buffer.sharedMemPerm.mode;
			if(regionPerm == READ_SHM || regionPerm == RW_SHM){
				if(shmflag == 0 && key != IPC_PRIVATE){
					shmid = allSharedMemRegions[i].shmid;
					return shmid;
				}
				else if(shmflag == IPC_CREAT){
					shmid = allSharedMemRegions[i].shmid;
					return shmid;
				}
				else{
					return -1;
				}
			}
			//IPC_CREAT | IPC_EXCL for already existing memory
			if(shmflag == (IPC_CREAT | IPC_EXCL)){
				return -1;
			}
		}
	}
	//Find unallocated shared memory region
	for(i = 0; i < SHARED_MEM_REGIONS; i++){
		if(allSharedMemRegions[i].key == -1){
			notused = i;
			notAllocFlag = 1;
			break;
		}
	}
	//no free shared memory region exists
	if(notused == -1 && !notAllocFlag){
		//cprintf("Error: no free memory region\n");
		return -1;
	}
	//Free shared memory exists
	if(notAllocFlag){
		
		if((key == IPC_PRIVATE) || (key == IPC_CREAT) || (key == (IPC_CREAT | IPC_EXCL))){
		//Allocate pages from free shared memory region
		for(int i = 0; i < pagesToAlloc; i++){
			void* newPage = kalloc();
			//Error in allocating page
			if(newPage == 0){
				//cprintf("error in allocating a page\n");
				return -1;
			}
			memset(newPage, 0, PGSIZE);
			allSharedMemRegions[notused].physicalAddress[i] = (void*)V2P(newPage);
		}
		allSharedMemRegions[notused].key = key;
		allSharedMemRegions[notused].size = pagesToAlloc;
		allSharedMemRegions[notused].buffer.sharedMemPerm.key = key;
		allSharedMemRegions[notused].buffer.sharedMemPerm.mode = currPermission;		
		allSharedMemRegions[notused].shmid = notused;
		allSharedMemRegions[notused].buffer.creatorPid = currProc->pid;
		allSharedMemRegions[notused].buffer.lastModifiedPid = -1;
		allSharedMemRegions[notused].buffer.sharedMemSize = pagesToAlloc;
		allSharedMemRegions[notused].buffer.nAttached = 0;
		shmid = notused;
		return shmid;
	}
	}
	return -1; //in case of any other combination of shmflag and key
}

//Returns index of process's sharedPages virtual address having virtual addr >= virtual address of current process
int getFirstAvailableIndex(struct proc *currProc, void* currVirtualAddr){
        //find starting address of segment or return -1
        int index=-1;
	int currProcVirtualAddress; 
	void* minVirtualAddr = (void*)(KERNBASE - 1);
	for(int i = 0; i < SHARED_MEM_REGIONS; i++){
		currProcVirtualAddress = (int)currProc->sharedPages[i].virtualAddress;
		if((currProcVirtualAddress >= (int)currVirtualAddr) && (currProc->sharedPages[i].key != -1) && (currProcVirtualAddress <= (int)minVirtualAddr)){
		minVirtualAddr = (void*)currProcVirtualAddress;//check if it works  
		index = i;
		break;
		}
	}
        return index;
}

void* shmatUtil(int shmid, void* shmaddr, int shmflag){
	void* minVirtualAddress = (void*)0;;
	void* virtualAddress = (void*)HEAPLIMIT;
	int roundedAddr;
	int index = -1;
	int currPerms;
	struct proc *currProc = myproc();
	index = allSharedMemRegions[shmid].shmid;
	//If shared memory region corresponding to shmid doesnt exist
	if(index == -1){
		return (void*)-1;	
	}
	//if invalid shmid
	if(shmid > SHARED_MEM_REGIONS || shmid < 0){
		return (void*)-1;
	}
	//todo: Rounding off shmaddr
	//If shmAddr is not given => shmaddr = NULL
	if(!shmaddr){
		for(int i=0; i<SHARED_MEM_REGIONS; i++){
			//pass limit to get first available index
			index = getFirstAvailableIndex(virtualAddress, currProc);
			if(index != -1){
				minVirtualAddress = currProc->sharedPages[index].virtualAddress;
				//found virtual address of shared mem region and it is valid also
				if(((int)virtualAddress + allSharedMemRegions[index].size*PGSIZE) <=  (int)minVirtualAddress){        
          			break;
          		}
          		else{
          			virtualAddress = (void*)((int)minVirtualAddress + currProc->sharedPages[index].size*PGSIZE);
          		}
			}
			//Index = -1 => didnt find min shared Mem region to attach
			else{
				break;
			}
		}
	}
	//Shmaddr is given
	else{
	//shmaddr is not within limits of [HEAPLIMIT, KERNBASE]
		if((int)shmaddr >= KERNBASE || (int)shmaddr < HEAPLIMIT) {
		      return (void*)-1;
		}
		roundedAddr = ((int)shmaddr & ~(SHMLBA - 1));
		
		//shmflag & SHM_RND != 0
		if(shmflag & SHM_RND){
			if(roundedAddr == 0){
			return (void*)-1;
			}
			virtualAddress = (void*)(roundedAddr);
		}
		//shmflag & SHM_RND == 0
		else{
		//shmaddr is page aligned
			if(roundedAddr == (int)shmaddr){
				virtualAddress = shmaddr;
			}
		}
	}
	//if base address + memory given exceeds the kernbase
	if(((int)virtualAddress + allSharedMemRegions[index].size * PGSIZE) >= KERNBASE){
		return (void*)-1;
	}
	//Find virtual addr of page having adress <= current virtual address
	int found = -1;
	for(int i = 0; i < SHARED_MEM_REGIONS; i++){
		if((currProc->sharedPages[i].key != -1) && ((int)virtualAddress < (int)currProc->sharedPages[i].size*PGSIZE) && ((int)virtualAddress >= (int)currProc->sharedPages[i].virtualAddress)){
		found = i;
		break;
		}
	}
	if(found == -1){
		return (void*)-1;
	}
	if((shmflag & SHM_RDONLY)||(allSharedMemRegions[found].buffer.sharedMemPerm.mode == READ_SHM)){
		currPerms = PTE_U;
	}
	else if(allSharedMemRegions[found].buffer.sharedMemPerm.mode == RW_SHM){
		currPerms = PTE_U | PTE_W;
	}
	else if(shmflag == SHM_REMAP){
		return (void*)0;
	}
	else{
	return (void*)-1;
	}
	//Mappages in virtual address space
	for(int i = 0; i < allSharedMemRegions[found].size; i++){
		int ret = mappages(currProc->pgdir, (void*)((int)virtualAddress + (i*PGSIZE)), PGSIZE, (int)allSharedMemRegions[found].physicalAddress[i], currPerms);
		//couldnt do mappings
		if(ret < 0) {
      deallocuvm(currProc->pgdir,(int)virtualAddress,(int)(virtualAddress + allSharedMemRegions[found].size));
      return (void*)-1;
	}
	int found2 = -1;
	//find free pages
	for(int i = 0; i < SHARED_MEM_REGIONS; i++){
		if(currProc->sharedPages[i].key == -1){
			found2 = i;
			break;
		}
	}
	if(found2 == -1){
		return (void*)-1;
	}
	else{
		currProc->sharedPages[found2].key = allSharedMemRegions[found].key;
		currProc->sharedPages[found2].size = allSharedMemRegions[found].size;
		currProc->sharedPages[found2].permission = currPerms;
		currProc->sharedPages[found2].virtualAddress = virtualAddress;
 		allSharedMemRegions[found].buffer.nAttached++;
 		allSharedMemRegions[found].buffer.lastModifiedPid = currProc->pid; 		
	}
}
return virtualAddress;
}
//shmdt
//Detach shared memory segment from current process
void* shmdtUtil(void* shmaddr){
	int totalSize=0;
	int i;
	//int found = -1;
	struct proc *currProcess = myproc();
	void* virtualAddress = 0;
	//finding the virtual address where memory is shared
	for(i=0; i<SHARED_MEM_REGIONS; i++){
		if(currProcess->sharedPages[i].key != -1 && currProcess->sharedPages[i].virtualAddress == shmaddr){
			virtualAddress = currProcess->sharedPages[i].virtualAddress;
			totalSize = currProcess->sharedPages[i].size;
			//found = i;
			break;
		}
	}
	// iF found, free the memory and return else return -1
	if(virtualAddress != 0){
	//Remove page table entry from pageTage
		for(int j = 0; j < totalSize; j++){
			pte_t* pageTableEntry = walkpgdir(currProcess->pgdir, (void*)((int)virtualAddress + j*PGSIZE), 0);
			//PTE doesnt exists
			if(!pageTableEntry){
				return (void*)-1;
			}
			*pageTableEntry = 0;
		}
		//reinitializing values of shared pages
		currProcess->sharedPages[i].size=0;
		currProcess->sharedPages[i].virtualAddress= (void*)0;
		currProcess->sharedPages[i].key=-1;
		currProcess->sharedPages[i].shmid=-1;

		if(allSharedMemRegions[i].buffer.nAttached == 0 && allSharedMemRegions[i].toDelete == 1){
			//freeing up the shared address space
			for(int j=0; j<allSharedMemRegions[i].size; j++){
				char *addressOfRegion = (char*)P2V(allSharedMemRegions[i].physicalAddress[i]);
				kfree(addressOfRegion);
				allSharedMemRegions[i].physicalAddress[i] = (void *)0;
			}
			
			// reinitializing shared memory regions
			allSharedMemRegions[i].size = 0;
			allSharedMemRegions[i].shmid = -1;
			allSharedMemRegions[i].key = -1;
			allSharedMemRegions[i].valid = 0;
			allSharedMemRegions[i].toDelete = 0;
			allSharedMemRegions[i].buffer.sharedMemPerm.key = -1;
			allSharedMemRegions[i].buffer.sharedMemPerm.mode = 0;
			allSharedMemRegions[i].buffer.nAttached = 0;
			allSharedMemRegions[i].buffer.sharedMemSize = 0;
			allSharedMemRegions[i].buffer.creatorPid = -1;
			allSharedMemRegions[i].buffer.lastModifiedPid = currProcess->pid;		
		}
		return (void*)0;
		
	}
	return (void*)-1;
}



//shmctl
//Control operations on shared memory according to cmd value

int shmctlUtil(int shmid, int cmd, void* buff){
	int index;
	int currPerm;
	int givenPerm;
	//Check if shmid is within range
	if(shmid > SHARED_MEM_REGIONS || shmid < 0)
		return -1;
	struct shmidDs * buffer = (struct shmidDs *)buff;

	index = allSharedMemRegions[shmid].shmid;
	//Check if invalid memory segment
	if(index == -1){
		return -1;
	}
	currPerm = allSharedMemRegions[shmid].buffer.sharedMemPerm.mode;
	givenPerm = buffer->sharedMemPerm.mode;
	switch(cmd){
	case IPC_STAT:
	case SHM_STAT:
	//copy info from kernel DS associated with shmid into shmDS pointed to by buff
		if(!buffer)
			return -1;
		else{
		if((currPerm == READ_SHM)||(currPerm == RW_SHM)){
			buffer->sharedMemSize = allSharedMemRegions[index].buffer.sharedMemSize;
			buffer->nAttached = allSharedMemRegions[index].buffer.nAttached;
			buffer->creatorPid = allSharedMemRegions[index].buffer.creatorPid;
			buffer->lastModifiedPid = allSharedMemRegions[index].buffer.lastModifiedPid;
			buffer->sharedMemPerm.mode = currPerm;	
			return 0; //no error		
		}
		else{
		return -1;
		}
		}
		break;
	
/*	case SHM_STAT_ANY:*/
/*		break;*/
	case IPC_SET:
		//write values of members of shmidDs pointed to by buf to kernel DS associated with this sharedMem
		if(!buffer){
			return -1;
		}
		else{
			//Change mode in permissions to this buffer's mode
			if((givenPerm == READ_SHM) || (givenPerm == RW_SHM)){
				allSharedMemRegions[index].buffer.sharedMemPerm.mode = givenPerm;
				return 0; //no error
			}
			else{
			return -1;
			}
		}
		break;
	case IPC_RMID:
	//Mark segment to be deleted - remove the shared memory segment
		//Mark segment toDelete = 1 if nAttached becomes 0, and caller process deletes it
		//Free all pages of current process
		if(allSharedMemRegions[index].buffer.nAttached == 0){
			for(int i = 0; i < allSharedMemRegions[index].size; i++){
				char* currAddr = (char*)P2V(allSharedMemRegions[index].physicalAddress[i]);
				kfree(currAddr);
				allSharedMemRegions[index].physicalAddress[i] = (void*)0;
			}
		//Reset values to default
		allSharedMemRegions[index].key = -1;
		allSharedMemRegions[index].size = 0;
		allSharedMemRegions[index].shmid = -1;
		allSharedMemRegions[index].valid = 0;
		allSharedMemRegions[index].toDelete = 0;
		allSharedMemRegions[index].buffer.sharedMemPerm.key = -1;
		allSharedMemRegions[index].buffer.sharedMemPerm.mode = 0;
		allSharedMemRegions[index].buffer.nAttached = 0;
		allSharedMemRegions[index].buffer.sharedMemSize = 0;
		allSharedMemRegions[index].buffer.creatorPid = -1;
		allSharedMemRegions[index].buffer.lastModifiedPid = -1;
		}
		else{
		//mark shared memory to be deleted
			allSharedMemRegions[index].toDelete = 1;
		}
		return 0;
		break;
/*	case IPC_INFO:*/
/*		break;*/
/*	case SHM_INFO:*/
/*		break;*/
/*	case SHM_LOCK:*/
/*		break;*/
/*	case SHM_UNLOCK:*/
/*		break;*/
	default:
		//retVal = -1;
		return -1;
		break;
	}
	return -1;
}

