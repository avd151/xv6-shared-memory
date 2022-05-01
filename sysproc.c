#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "shm.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//Shared Memory System calls

//Shmget
//int shmget(int key, int size, int shmflag)
int
sys_shmget(void){
	//cprintf("In shmget\n");
	int key;
	int size;
	int shmFlag;
	int ret;
	int shmid;
	ret = argint(0, &key);
	if(ret < 0)return -1;
	ret = argint(1, &size);
	if(ret < 0) return -1;
	ret = argint(2, &shmFlag);
	if(ret < 0)return -1;
	shmid = shmgetUtil(key, size, shmFlag);
	return shmid;
}

//Shmat
//void* shmat(int shmid, void* shmAddr, int shmflag)
void*
sys_shmat(void){
	//cprintf("In shmat\n");
	int shmid;
	int shmAddr;
	int shmFlag;
	int ret;
	void* shmStartAddr;
	ret = argint(0, &shmid);
	if(ret < 0)return (void*)-1;
        ret = argint(1, &shmAddr);
        if(ret < 0)return (void*)-1;
        ret = argint(2, &shmFlag);
        if(ret < 0)return (void*)-1;
	shmStartAddr = shmatUtil(shmid, (void*)shmAddr, shmFlag);
	return shmStartAddr;
}

//Shmdt
void*
sys_shmdt(void){
	//cprintf("In shmdt\n");
	int shmAddr;
	int ret;
	void* retAddr;
	ret = argint(0, &shmAddr);
	if(ret < 0)return (void*)-1;
	retAddr = shmdtUtil((void*)shmAddr);
	return retAddr;
}


//Shmctl
int
sys_shmctl(void){
	//cprintf("In shmctl\n");
	int shmid;
	int command;
	int buffer;
	int ret;
	ret = argint(0, &shmid);
	if(ret < 0)return -1;
   	ret = argint(1, &command);
    if(ret < 0)return -1;
    ret = argint(2, &buffer);
    if(ret < 0)return -1;
	ret = shmctlUtil(shmid, command, (void*)buffer);
	return ret;
}
