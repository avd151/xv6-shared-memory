#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	//Shmget
	int shmid = shmget(1, 2,3);
	if(shmid >= 0){
		printf(1, "shmid = %d\n", shmid);
	}else{
		printf(1, "Error in shmget\n");
	}

	//Shmat
	void* testAddr = (void*)0;
	void* shmAddr = shmat(1,testAddr,2);
       	if(shmAddr != (void*)-1){
		printf(1, "In shmat \n");
	}else{
		printf(1, "error in shmat\n");	
	}
	exit();
}
