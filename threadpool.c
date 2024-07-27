#include "threadpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define NUMBER 2

/* 任务结构体 */
typedef struct Task {
	void (*function)(void*);
	void* arg;
}Task;

/* 线程池结构体 */
typedef struct ThreadPool {
	/* 任务队列 */
	Task* taskQ;
	/* 任务队列容量 */
	int queueCapacity;
	/* 当前任务个数 */
	int queueSize;
	/* 队头->取数据 */
	int queueFront;
	/* 队尾->放数据 */
	int queueRear;

	/* 管理者线程id */
	pthread_t managerId;
	/* 工作线程id列表 */
	pthread_t* threadIds;
	/* 最小线程数 */
	int minNum;
	/* 最大线程数 */
	int maxNum;
	/* 忙线程数 */
	int busyNum;
	/* 存活线程数 */
	int aliveNum;
	/* 关闭线程数 */
	int exitNum;

	/* 锁整个线程池 */
	pthread_mutex_t mutexPool;
	/* 锁忙线程数 */
	pthread_mutex_t mutexBusy;
	/* 判定是不是满了，满了阻塞生产者 */
	pthread_cond_t notFull;
	/* 判定是不是空了，空了阻塞消费者 */
	pthread_cond_t notEmpty;

	/* 线程池是否关闭，0->没关，1->关了 */
	int shutdown;
}ThreadPool;

/* 创建线程池并初始化 */
ThreadPool* threadPoolCreate(int min, int max, int queueCapacity) {
	/* 给线程池申请内存 */
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do {
		if (pool == NULL) {
			printf("malloc threadpool fail...\n");
			break;
		}

		/* 申请工作线程的内存 */
		pool->threadIds = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->threadIds == NULL) {
			printf("malloc threadIds fail...\n");
			break;
		}
		memset(pool->threadIds, 0, max);

		/* 初始化线程池内部变量 */
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->exitNum = 0;
		pool->aliveNum = min;

		/* 初始化互斥锁和条件变量 */
		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0) {
			printf("mutex or condition init fail...\n");
			break;
		}

		/* 任务队列初始化 */
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueCapacity);
		if (pool->taskQ == NULL) {
			printf("malloc taskQ fail...\n");
			break;
		}
		pool->queueCapacity = queueCapacity;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		/* 初始化线程池关闭标记 */
		pool->shutdown = 0;

		/* 创建管理者线程 */
		pthread_create(&pool->managerId, NULL, manager, pool);
		/* 创建工作线程 */
		for (int i = 0; i < min; i++) {
			pthread_create(&pool->threadIds[i], NULL, worker, pool);
		}
	} while (0);

	/* 资源释放，只有当某项资源申请失败的时候，需要把线程池其他资源全部释放 */
	if (pool && pool->threadIds) free(pool->threadIds);
	if (pool && pool->taskQ) free(pool->taskQ);
	if (pool) free(pool);

	return NULL;
}

/* 给线程池添加任务 */
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg) {
	pthread_mutex_lock(&pool->mutexPool);
	while (pool->queueSize == pool->queueCapacity && !pool->shutdown) {
		/* 阻塞生产者 */
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	/* 判断当前线程池是否被关闭 */
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}

	/* 添加任务 */
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	/* 移动队尾 */
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	/* 唤醒阻塞的线程 */
	pthread_cond_signal(&pool->notEmpty);
	pthread_mutex_unlock(&pool->mutexPool);
}

/* 工作线程使用函数*/
void* worker(void* arg) {
	ThreadPool* pool = (ThreadPool*)arg;
	while (1) {
		/* 各个工作线程访问工作队列，此操作需要加锁 */
		pthread_mutex_lock(&pool->mutexPool);
		/* 判断当前工作队列是否为空 */
		while (pool->queueSize == 0 && !pool->shutdown) {
			/* 阻塞工作线程 */
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			/* 判断是否要销毁线程 */
			if (pool->exitNum > 0) {
				pool->exitNum--;
				if (pool->aliveNum > pool->minNum) {
					pool->aliveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}
		}
		/* 判断当前线程池是否被关闭 */
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}

		/* 从任务队列中取出任务 */
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;
		/* 移动头结点 */
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;

		/* 解锁，唤醒生产者 */
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);

		/* 修改忙线程数 */
		printf("thread %ld start working...\n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);
		/* 执行任务函数 */
		task.function(task.arg);
		free(task.arg);
		/* 修改忙线程数 */
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
		printf("thread %ld end working...\n", pthread_self());
	}

	return NULL;
}

/* 管理者线程使用函数 */
void* manager(void* arg) {
	ThreadPool* pool = (ThreadPool*)arg;

	while (!pool->shutdown) {
		/* 每隔5秒检测一次是否需要修改线程池工作线程数量 */
		sleep(5);

		/* 取出线程池中任务的数量和当前线程的数量 */
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int aliveNum = pool->aliveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		/* 取出忙线程的数量 */
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		/* 添加线程 */
		/* 任务的个数>存活的线程个数 && 存活的线程数<最大线程数 */
		if (queueSize > aliveNum && aliveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->aliveNum < pool->maxNum; i++) {
				if (pool->threadIds[i] == 0) {
					pthread_create(&pool->threadIds[i], NULL, worker, pool);
					counter++;
					pool->aliveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		/* 销毁线程 */
		/* 忙的线程*2 < 存活的线程数 && 存活的线程 > 最小线程数 */
		if (busyNum * 2 < aliveNum && aliveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);

			/* 让工作的线程自杀 */
			for (int i = 0; i < NUMBER; i++) {
				pthread_cond_signal(&pool->notEmpty);
			}
		}
	}

	return NULL;
}

/* 线程退出 */
void threadExit(ThreadPool* pool) {
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; i++) {
		if (pool->threadIds[i] == tid) {
			pool->threadIds[i] = 0;
			printf("threadExit() called, %ld exiting...\n", tid);
			break;
		}
	}
	pthread_exit(NULL);
}

/* 获取线程池中工作的线程个数 */
int threadPoolBusyNum(ThreadPool* pool) {
	pthread_mutex_lock(&pool->mutexPool);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexPool);

	return busyNum;
}

/* 获取线程池中活着的线程个数 */
int threadPoolAliveNum(ThreadPool* pool) {
	pthread_mutex_lock(&pool->mutexPool);
	int aliveNum = pool->aliveNum;
	pthread_mutex_unlock(&pool->mutexPool);

	return aliveNum;
}

/* 销毁线程池 */
int threadPoolDestroy(ThreadPool* pool) {
	if (!pool) return -1;

	/* 关闭线程池 */
	pool->shutdown = 1;
	/* 阻塞回收管理者线程 */
	pthread_join(pool->managerId, NULL);
	/* 唤醒阻塞的消费者线程 */
	for (int i = 0; i < pool->aliveNum; i++) {
		pthread_join(pool->threadIds[i], NULL);
	}

	/* 释放堆内存 */
	if (pool->taskQ) free(pool->taskQ);
	if (pool->threadIds) free(pool->threadIds);
	free(pool);

	/* 销毁互斥锁和条件变量 */
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);

	pool = NULL;
	return -1;
}
