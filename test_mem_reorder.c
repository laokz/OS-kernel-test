/*
 * Thread0		Thread1
 *	CPU0		CPU2
 *	------------------
 *	x0=2;		x1=2;
 *	r2=x1;		r2=x0;
 *
 * Concurrent threads p0, p1 run on different CPUs. They all store one variable
 * then load another variable respectively. It looks like the final r2 value
 * should not be both 0. 
 *
 * But, run this several tens(maybe no more than 40 times),
 * you may be lucky to see both r2==0 in x86_64.
 * That is the causality of MEMORY REORDERING!
 *
 * Copyright || Copyleft (c) 2019 laokz@foxmail.com
 *
 * Thank Paul E. McKenney && his perfbook "Is Parallel Programming Hard,
 * And, If So, What Can You Do About It?"
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>

/* Here comes from perfbook. */
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#define READ_ONCE(x) \
                ({ typeof(x) ___x = ACCESS_ONCE(x); ___x; })
#define WRITE_ONCE(x, val) \
                do { ACCESS_ONCE(x) = (val); } while (0)
#define smp_mb() \
__asm__ __volatile__("mfence" : : : "memory")

int x0, x1;

/* Based on perfbook's Listing 15.1 */
void* p0(void *x)
{
	int r2;
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(0, &set);
	CPU_SET(1, &set);
	if (sched_setaffinity(0, sizeof(set), &set) == -1) {
		perror("setaffinity");
		return NULL;
	}

	WRITE_ONCE(x0, 2);
	r2 = READ_ONCE(x1);

	printf("p0: r2=%d     ", r2);
	return NULL;
}

void* p1(void *x)
{
	int r2;
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(2, &set);
	CPU_SET(3, &set);
	if (sched_setaffinity(0, sizeof(set), &set) == -1) {
		perror("setaffinity");
		return NULL;
	}

	WRITE_ONCE(x1, 2);
	r2 = READ_ONCE(x0);

	printf("p1: r2=%d     ", r2);
	return NULL;
}

int main(void)
{
	int i, n=10;
	pthread_t pid0, pid1;

	for (i = 0; i < n; i++) {
		if (pthread_create(&pid0, NULL, p0, NULL)) {
			perror("pthread_create:p0");
			exit(1);
		}
		if (pthread_create(&pid1, NULL, p1, NULL)) {
			perror("pthread_create:p1");
			exit(1);
		}
		if (pthread_join(pid0, NULL)) {
			perror("pthread_join:p0");
			exit(1);
		}
		if (pthread_join(pid1, NULL)) {
			perror("pthread_join:p1");
			exit(1);
		}

		x0 = 0;
		x1 = 0;
		smp_mb();	/* make sure x0, x1 reset to 0 from all CPUs */
		printf("%d\n", i);
	}
}
