#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	int shmid = shmget(1, 2,3);
	if(shmid >= 0)
		printf(1, "shmid = %d\n", shmid);
	else
		printf(1, "Error in shmget\n");
 	exit();
}
