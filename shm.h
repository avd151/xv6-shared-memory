#define IPC_CREAT 010000
#define IPC_EXCL 020000
#define IPC_PRIVATE 030000
#define SHARED_MEM_REGIONS 64

#define	SHM_RDONLY	040000	
#define	SHM_RND		050000
#define	SHM_REMAP	060000
#define	SHM_EXEC	070000

#define	SHMLBA	(1 * PGSIZE) //multiple of page size

//Shared Memory Region
struct sharedMemRegion{
	int key;
	int size;
	int shmid;
	int valid; //0 = invalid, 1 = valid
	void* physicalAddress[SHARED_MEM_REGIONS]; //check: single variable or array of physical addresses of shared memory regions
};

 
void sharedMemoryInit(void);
int shmgetUtil(int, int, int); //key, size, shmflag
void* shmatUtil(int, void*, int); //shmid, shmaddr, shmflag
//int shmdet(void*); //shmaddr
//int shmctl(int, int, struct shmid_ds*); //shmid, cmd, struct shmid_ds* buf
