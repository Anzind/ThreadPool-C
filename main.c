#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include"threadpool.h"

/* 任务函数 */
void taskFuntion(void* arg) {
	int num = *(int*)arg;
	printf("thread %ld is working, number = %d\n", pthread_self(), num);
	sleep(1);
}

int main() {
	/* 创建一个线程池 */
	ThreadPool* pool = threadPoolCreate(3, 10, 100);
	for (int i = 0; i < 100; i++) {
		int* num = (int*)malloc(sizeof(int));
		*num = i + 100;
		threadPoolAdd(pool, taskFuntion, num);
	}
	sleep(30);

	threadPoolDestroy(pool);
	return 0;
}