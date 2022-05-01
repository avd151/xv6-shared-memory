//for shmget
#define IPC_CREAT 01000
#define IPC_EXCL 02000
#define IPC_PRIVATE 00000

#define SHARED_MEM_REGIONS 64

//for shmat
#define	SHM_RDONLY	010000	
#define	SHM_RND		020000
#define	SHM_REMAP	040000
#define	SHM_EXEC	0100000

//for shmctl
#define IPC_STAT 0
#define SHM_STAT 1
#define SHM_STAT_ANY 2
#define IPC_SET 3
#define IPC_RMID 4
#define IPC_INFO 5
#define SHM_INFO 6
#define SHM_LOCK 7
#define SHM_UNLOCK 8

//shmflags
#define READ_SHM 04
#define RW_SHM 06

#define	SHMLBA	(1 * PGSIZE) //multiple of page size

//Permissions 
struct permission{
	int mode; //READ, WRITE, READ_WRITE
	int key; 
};
 
//Shmid DS 
struct shmidDs{
	struct permission sharedMemPerm;
	int sharedMemSize;
	//todo: time, date of last modified
/*	int creationTime;*/
/*	int attachTime;*/
/*	int detachTime;*/
	int nAttached;
	int creatorPid;
	int lastModifiedPid;
};

//Shared Memory Region
struct sharedMemRegion{
	int key;
	int size;
	int shmid;
	int valid; //0 = invalid, 1 = valid
	int toDelete;
	void* physicalAddress[SHARED_MEM_REGIONS]; //check: single variable or array of physical addresses of shared memory regions
	struct shmidDs buffer;
};

//Shared Memory Regions Array
struct shmArr {
	struct spinlock *s;
	struct sharedMemRegion allSharedMemRegions [SHARED_MEM_REGIONS];
};
 

void sharedMemoryInit(void);
int shmgetUtil(int, int, int); //key, size, shmflag
void* shmatUtil(int, void*, int); //shmid, shmaddr, shmflag
void* shmdtUtil(void*); //shmaddr
int shmctlUtil(int, int, void*); //shmid, cmd, struct shmidDS buf 
