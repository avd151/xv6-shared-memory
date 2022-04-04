#define IPC_CREAT 1
#define IPC_EXCL 2
#define IPC_PRIVATE 3
#define SHARED_MEM_REGIONS 64
//todo: check if can define structures for shared memory pages and regions here, and redeclare shared memory system call prototypes 
void shared_memory_init(void);
int shmget_util(int, int, int); //key, size, shmflag
//int shmat(int, void*, int); //shmid, shmaddr, shmflag
//int shmdet(void*); //shmaddr
//int shmctl(int, int, struct shmid_ds*); //shmid, cmd, struct shmid_ds* buf
