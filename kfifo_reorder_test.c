/*
 * This is a test for linux KFIFO reorder bug.
 * Target weak memory order architecture: ARMv7
 *
 * Copyright || Copyleft (c) 2019 laokz@foxmail.com
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>

#include "kfifo_arm.h"	/* tailored kernel source for ARMv7 */
DEFINE_KFIFO(fifo, int, 16);

static int N = 1000000;	/* rw times */
static int count = 0;	/* reorder counts */

/* set affinity of current thread to cpu #id */
void setcpu(int id)
{
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(id, &set);
	if (sched_setaffinity(0, sizeof(set), &set) == -1) {
		perror("setaffinity");
		exit(1);
	}
}

/* writer */
static void* p0(void *data)
{
	int i;
	setcpu(0);
	for (i = 1; i <= N; i++)
		while (!kfifo_put(&fifo, i));
	return NULL;
}

/* reader */
static void* p1(void *data)
{
	int i, d;
	setcpu(2);
	for (i = 1; i <= N; i++) {
		while(!kfifo_get(&fifo, &d));
		if (d != i) {
			count++;
			printf("expected=%-10d got=%-10d : %s\n",
					i, d, i > d ? "stale data" : "future data");
		} else if (i % 1000000 == 0) {
			printf("%d\n", i);
		}
	}
	return NULL;
}

/* usage: kfifo_reorder_test [rw times] */
int main(int argc, char **argv)
{
	int i;
	pthread_t pid0, pid1;

	if (argc > 1) {
		i = atoi(argv[1]);
		if (i > 0)
			N = i;
	}

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

	printf("total:%d  reorder:%d\n", N, count);
}
