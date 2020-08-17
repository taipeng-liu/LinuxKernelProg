#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "userapp.h"

#define NUM_OF_JOB 5

char cmd[100];
char buf[100];

int print_pid(void) {
	printf("(PID = %d)", getpid());
	return 0;
}

/* READ DATA FROM /proc/mp2/status, SAVE INTO buf */
int proc_sprint(char *buf) {
	int fd = open("/proc/mp2/status", O_RDONLY);
	int len = 0;

	memset(buf, 0, 100);
	len = read(fd, buf, 100);
	close(fd);

	return len;
}

/* READ DATA FROM /proc/mp2/status, PRINT DATA */
int proc_print(void) {
	proc_sprint(buf);
	printf("%s", buf);

	return 0;
}

/* REGISTRATION */
int proc_reg(int pid, unsigned long period, unsigned long computation) {
	FILE *fp = fopen("/proc/mp2/status", "w+");
	fprintf(fp, "R, %d, %lu, %lu", pid, period, computation);
	
	fclose(fp);
	return 0;
}

/* YIELD */
int proc_yield(int pid) {
	FILE *fp = fopen("/proc/mp2/status", "w+");
	fprintf(fp, "Y, %d", pid);
	
	fclose(fp);
	return 0;
}

/* DE-REGISTARTION */
int proc_dereg(int pid) {
	FILE *fp = fopen("/proc/mp2/status", "w+");
	fprintf(fp, "D, %d", pid);
	
	fclose(fp);
	return 0;
}

/* CHECK IF REGISTARTION SUCCEEDS */
int list_contain_pid(char *buf, pid_t pid) {
	char temp[100];
	snprintf(temp, 100, "%d", pid);

	if (strstr(buf, temp)) 
		return 1;
	else 
		return 0;
}

/* MAIN JOB IN THE WHILE LOOP */
unsigned long do_factorial_job(int i, struct timeval t_pstart) {
	unsigned long  j = 1, fact = 1;
	struct timeval t_start, t_end;

	gettimeofday(&t_start, NULL);

	print_pid();
	printf("JOB %d START     | ", i);
	printf("WAKEUP TIME = %lu\n", t_start.tv_sec - t_pstart.tv_sec);

	for(; j <= 1000000000; j++) {
		fact = (fact * j) / (2048 * 2048);
	}

	gettimeofday(&t_end, NULL);

	print_pid();
	printf("PROCESS TIME = %lu\n", t_end.tv_sec - t_start.tv_sec);
	print_pid();
	printf("JOB %d COMPLETED | ", i);
	printf("SLEEP  TIME = %lu\n", t_end.tv_sec - t_pstart.tv_sec);
	
	return fact;
}

/* MAIN */
int main(int argc, char* argv[])
{
	char *p;
	int pid = getpid();
	unsigned long period, computation = 4000;
	int i, num_of_job;
	struct timeval t_pstart, t_pend;

	//Read period and num_of_job from arguments
	if (argc < 3) {
		printf("USAGE: %s [PERIOD(ms)] [NUM_OF_JOB]\n", argv[0]);
		exit(1);
	}
	period = strtoul(argv[1], &p, 10);
	num_of_job = strtol(argv[2], &p, 10);
	printf("PID = %d, period = %lu, compute_time = %lu, num_of_job = %d\n", pid, period, computation, num_of_job);

	//Save start time of the process
	gettimeofday(&t_pstart, NULL);

	//register
	print_pid();
	printf("***Call register***\n");
	proc_reg(pid, period, computation);

	//verify
	proc_sprint(buf);
	if (!list_contain_pid(buf, pid)) {
		print_pid();
		printf("Registration is denied, please adjust period.\n");
		exit(1);
	}

	//call yield
	print_pid();
	printf("***Call yield before running jobs***\n");
	proc_yield(pid);
	//proc_print();

	//Run jobs
	for (i = 0; i < num_of_job; i++) {

		do_factorial_job(i, t_pstart);

		print_pid();
		printf("***Call yield***\n\n");
		proc_yield(pid);
		//proc_print();
	}

	//de-register
	print_pid();
	printf("***Call dereg***\n");
	proc_dereg(pid);
	//proc_print();

	gettimeofday(&t_pend, NULL);
	print_pid();
	printf("TOTAL PROCESS TIME = %lu\n", t_pend.tv_sec - t_pstart.tv_sec);
	
	return 0;
}

