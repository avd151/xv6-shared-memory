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

//shmat tests
void shmatUtilTests(){
	int atId1 = shmgetUtil(2345, 1745, 06 | IPC_CREAT);
	int atId2 = shmgetUtil(5069, 1745, 06 | IPC_CREAT);
	int atId3 = shmgetUtil(3853, 1745, 06 | IPC_CREAT);
	if(atId1 == -1 || atId2 == -1 || atId3 == -1){
		printf("shmgetUtil Failed");
		return;
	}
	
	//Basic shmat test
	printf("Basic memory attachment : ");
	char* test1 = (char*)shmatUtil(atId1, (void*)0,0)
	if((int)test1 == -1){
		printf("Failed\n");
		return;
	}
	else{
		printf("Passed\n");
	}
	int test1dt1 = shmdtUtil(test1s1);
	if(test1dt1 == -1){
		printf("shmdtUtil Failed");
		return;
	}
	
	//Checking for address less than heap limit
	printf("Memory access below HeapLimit : ");
	char *test = (char*)shmatUtil(atId1,(void*)(HEAPLIMIT - 20), 0);
	if((int)test == -1){
		printf("Passed\n");
	}
	else{
		printf("Failed\n");
	}

	//Checking for address more than kernbase
	printf("Memory access beyond Kernbase: ");
	test = (char*)shmatUtil(atId1,(void*)(KERNBASE + 20), 0);
	if((int)test == -1){
		printf("Passed\n");
	}
	else{
		printf("Failed\n");
	}

	//Invalid Shmid within allowed range[0,64]
	printf("Invalid Shmid within allowed range: ");
	test = (char*)shmatUtil(24,(void*)0, 0);
	if((int)test == -1){
		printf("Passed\n");
	}
	else{
		printf("Failed\n");
	}


	//Invalid Shmid within allowed range[0,64]
	printf("Invalid Shmid beyond allowed range: ");
	test = (char*)shmatUtil(179,(void*)0, 0);
	if((int)test == -1){
		printf("Passed\n");
	}
	else{
		printf("Failed\n");
	}
	return;
}
