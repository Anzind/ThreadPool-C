#include "threadpool.h"

#include <pthread.h>

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



}ThreadPool;