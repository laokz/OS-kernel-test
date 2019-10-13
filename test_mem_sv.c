/*
 * Thread0		Thread1		Thread2		Thread3
 *	CPU0		CPU2		CPU1		CPU3
 *	--------------------------------------------
 *	x=1			x=2
 *							r1=x		r3=x
 *							r2=x		r4=x
 *
 * Concurrent threads p0, p1, p2, p3 run on different CPUs. p0, p1 write the
 * variable x, and p2, p3 read the same variable twice respectively. 
 *
 * p2, p3 should perceive the same result according cache cohenrence. That
 * is it is impossible while r1==1 && r2==2 && r3==2 && r4==1 ||
 * r1==2 && r2==1 && r3==1 && r4==2.
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

int x, r1, r2, r3, r4;

void setcpu(int id);

/* Based on perfbook's Listing 15.15 */
void* p0(void *p)
{
	setcpu(0);
	WRITE_ONCE(x, 1);
}

void* p1(void *p)
{
	setcpu(2);
	WRITE_ONCE(x, 2);
}

void* p2(void *p)
{
	setcpu(1);
	r1 = READ_ONCE(x);
	r2 = READ_ONCE(x);
}

void* p3(void *p)
{
	setcpu(3);
	r3 = READ_ONCE(x);
	r4 = READ_ONCE(x);
}

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

#define N 2000000	// torture times

int main(void)
{
	int i, j, k, m;
	int count[3][3][3][3];	// count[r1][r2][r3][r4]
	pthread_t pid0, pid1, pid2, pid3;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			for (k = 0; k < 3; k++)
				for (m = 0; m < 3; m++)
					count[i][j][k][m] = 0;

	for (i = 0; i < N; i++) {
		if (pthread_create(&pid0, NULL, p0, NULL)) {
			perror("pthread_create:p0");
			exit(1);
		}
		if (pthread_create(&pid1, NULL, p1, NULL)) {
			perror("pthread_create:p1");
			exit(1);
		}
		if (pthread_create(&pid2, NULL, p2, NULL)) {
			perror("pthread_create:p2");
			exit(1);
		}
		if (pthread_create(&pid3, NULL, p3, NULL)) {
			perror("pthread_create:p3");
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
		if (pthread_join(pid2, NULL)) {
			perror("pthread_join:p1");
			exit(1);
		}
		if (pthread_join(pid3, NULL)) {
			perror("pthread_join:p1");
			exit(1);
		}

		count[r1][r2][r3][r4]++;

		x = r1 = r2 = r3 = r4 = 0;
	}

	printf("r1 r2 r3 r4\n");
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			for (k = 0; k < 3; k++)
				for (m = 0; m < 3; m++)
					if (count[i][j][k][m])
						printf("%d  %d  %d  %d  :%6d\n", i, j, k, m,
								count[i][j][k][m]);
}

/* In i5-2450M laptop, resulting:
r1 r2 r3 r4
0  0  0  0  :  2389
0  0  0  1  :     3
0  0  0  2  :     3
0  0  1  1  :  1982
0  0  2  1  :     1
0  0  2  2  :  1431
1  1  0  0  :   315
1  1  0  1  :     1
1  1  0  2  :     2
1  1  1  1  : 18649
1  1  1  2  :    29
1  1  2  1  :     2
1  1  2  2  :507277
2  1  1  1  :     3
2  1  2  2  :     1
2  2  0  0  :   227
2  2  1  1  :932269
2  2  1  2  :     3
2  2  2  1  :    29
2  2  2  2  :535384
*/
