#include "types.h"
#include "stat.h"
#include "user.h"
#include "memlayout.h"
#include "shm.h"
void shmatTests();

int
main(int argc, char *argv[])
{
	/*
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
	*/
	shmatTests();
	exit();
}

//shmat tests
void shmatTests(){
	int atId1 = shmget(2345, 1745, 06 | IPC_CREAT);
	int atId2 = shmget(5069, 1745, 06 | IPC_CREAT);
	int atId3 = shmget(3853, 1745, 06 | IPC_CREAT);
	if(atId1 == -1 || atId2 == -1 || atId3 == -1){
		printf(1, "shmget Failed");
	}
	
	//Basic shmat test
	printf(1, "Basic memory attachment : ");
	char* test1 = (char*)shmat(atId1, (void*)0,0);
	if((int)test1 == -1){
		printf(1, "Failed\n");
	}
	else{
		printf(1, "Passed\n");
	}
	int test1dt1 = shmdt(test1);
	if(test1dt1 == -1){
		printf(1, "shmdt Failed");
	}
	
	//Checking for address less than heap limit
	printf(1, "Memory access below HeapLimit : ");
	char *test = (char*)shmat(atId1,(void*)(HEAPLIMIT - 20), 0);
	if((int)test == -1){
		printf(1, "Passed\n");
	}
	else{
		printf(1, "Failed\n");
	}

	//Checking for address more than kernbase
	printf(1, "Memory access beyond Kernbase: ");
	test = (char*)shmat(atId1,(void*)(KERNBASE + 20), 0);
	if((int)test == -1){
		printf(1, "Passed\n");
	}
	else{
		printf(1, "Failed\n");
	}

	//Invalid Shmid within allowed range[0,64]
	printf(1,"Invalid Shmid within allowed range: ");
	test = (char*)shmat(24,(void*)0, 0);
	if((int)test == -1){
		printf(1, "Passed\n");
	}
	else{
		printf(1, "Failed\n");
	}


	//Invalid Shmid within allowed range[0,64]
	printf(1, "Invalid Shmid beyond allowed range: ");
	test = (char*)shmat(179,(void*)0, 0);
	if((int)test == -1){
		printf(1, "Passed\n");
	}
	else{
		printf(1, "Failed\n");
	}
	return;
}
