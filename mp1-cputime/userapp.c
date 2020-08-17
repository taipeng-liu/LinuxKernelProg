#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "userapp.h"

int proc_reg() {
	FILE *fp = fopen("/proc/mp1/status", "w+");
	fprintf(fp, "%d", getpid());
	
	fclose(fp);
	return 0;
}

int proc_print() {
	char buf[100] = {0};
	int fd = open("/proc/mp1/status", O_RDONLY);
	
	read(fd, buf, 100);
	printf("%s", buf);
	close(fd);
	return 0;
}

int main(int argc, char* argv[])
{
	long i, j, fact;

	proc_reg();

	for (i = 0; i < 5; i++) {
		fact = 1;
		for(j = 1; j <= 1000000000; j++) {
			fact = (fact * j) / (2048 * 2048);
		}
		proc_print();
	}

	return 0;
}

