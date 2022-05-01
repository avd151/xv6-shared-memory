#include "types.h"
#include "stat.h"
#include "user.h"
#include "memlayout.h"
#include "shm.h"
void shmgetTests();
void shmatTests();
void shmdtTests();
void shmctlTests();

int
main(int argc, char *argv[]){
	shmgetTests();
	shmatTests();
	shmdtTests();
	shmctlTests();
	exit();
}

//shmget tests
void shmgetTests(){
	printf(1,"Test for shmget\n\n");
	//Basic shmget test
	printf(1, "Basic memory creation: ");
	int test = shmget(3846, 2350, 06 | IPC_CREAT);
	if(test < 0) {
		printf(1, "Failed\n");
	} else {
		printf(1, "Passed\n");
	}

	//getting existing shmid
	printf(1, "Already created memory's shmid: ");
	int test1 = shmget(3846, 2350, 0);
	if(test1 == test){
		printf(1, "Passed\n");
	} else {
		printf(1, "Failed\n");
	}	

	//checking for zero size
	printf(1,"Region with zero size: ");
	int test2 = shmget(4001, 0, 06 | IPC_CREAT);
	if(test2 < 0) {
		printf(1, "Passed\n");
	} else {
		printf(1, "Failed\n");
	}

	//checking for no permissions
	printf(1, "Invalid permission check : ");
	int test3 = shmget(2005, 4000, IPC_CREAT);
	if(test3 < 0) {
		printf(1, "Passed\n");
	} else {
		printf(1, "Failed\n");
	}

	//checking for more than allowed pages
	printf(1,"More than allowed pages: ");
	int test4 = shmget(3825, 1.6e+7 + 17, 06 | IPC_CREAT);
	if(test4 < 0) {
		printf(1, "Passed\n");
	} else {
		printf(1, "Failed\n");
	}
	return;
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

	//Read only access test
	printf(1, "Read only access test: ");
	test = (char*)shmat(atId2, (void *)(0), SHM_RDONLY);
	if((int)test != -1) {
		printf(1,"Allowed ! : Passed\n");
	}
	else {    
		printf(1, "Not allowed ! : Failed\n");
	}
	printf(1, "Detaching readonly region : ");
   	int testdt = shmdt(test);
    	if(testdt < 0) {
		printf(1, "Failed\n");
	} 
	else {
       		printf(1,"Passed\n");
    }
	return;
}

//shmdt tests
void shmdtTests(){
	printf(1,"Test for shmdt\n\n");
	//basic detach 
	printf(1,"Basic detach test: ");
	//int test1 = shmget(2473, 2647, 06 | IPC_CREAT);
	char* testaddr1 = (char *)shmat(20, (void *)0, 0);
	printf(1, "Basic detach test : ");
	int testdt = shmdt(testaddr1);
    	if(testdt < 0) {
		printf(1, "Failed\n");
	}
	else {
		printf(1,"Passed\n");
	}

	//detaching invalid virtual address
	int testdt2 = shmdt((void*)7538);
	if(testdt2 < 0) {
		printf(1, "Passed\n");
	}
	else {
		printf(1,"Failed\n");
	}
	return;
}

void shmctlTests(){

	int shmid = 30;
	// user shmid_ds data structure
	struct shmidDs buff1;
	printf(1, "Test for IPC_STAT : ");
	int ctl = shmctl(shmid, IPC_STAT, &buff1);
	if(ctl < 0) {
		printf(1, "Test Failed\n");
	} else {
		printf(1, "Test Passed\n");
	}
	// readonly
	buff1.sharedMemPerm.mode = 04;
	printf(1, "Test for IPC_SET: ");
	// set read-only permission to exisiting region
	ctl = shmctl(shmid, IPC_SET, &buff1);
	if(ctl < 0) {
		printf(1, "Test Failed\n");
	} else {
		printf(1, "Test Passed\n");
	}
	printf(1, "Test for IPC_RMID: ");
	int ctl3 = shmctl(12345, IPC_RMID, (void *)0);
	if(ctl3 < 0) {
		printf(1, "Test Passed\n");
	} else {
		printf(1, "Test Failed\n");
	}	
return;
}
