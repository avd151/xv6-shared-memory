#define IPC_CREAT 1
#define IPC_EXCL 2
#define IPC_PRIVATE 3
#define SHARED_MEM_REGIONS 64

//Shared Memory Region
struct sharedMemRegion{
	int key;
	int size;
	int shmid;
	int valid; //0 = invalid, 1 = valid
	void* physicalAddress[SHARED_MEM_REGIONS]; //array of physical addresses of shared pages of a region
};

 
void shared_memory_init(void);
int shmget_util(int, int, int); //key, size, shmflag
void* shmat(int, void*, int); //shmid, shmaddr, shmflag
//int shmdet(void*); //shmaddr
//int shmctl(int, int, struct shmid_ds*); //shmid, cmd, struct shmid_ds* buf
