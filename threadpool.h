/**
	线程池类
*/

#pragma once
#include "TaskQueue.h"
#include "TaskQueue.cpp"

template<typename T>
class ThreadPool {
public:
	/* 创建线程池 */
	ThreadPool(int min, int max);
	/* 销毁线程池 */
	~ThreadPool();

	/* 给线程池添加任务 */
	void addTask(Task<T> task);

	/* 获取线程池中忙的线程个数 */
	int getBusyNum();

	/* 获取线程池中活着的线程个数 */
	int getAliveNum();

private:
	/* 任务队列 */
	TaskQueue<T>* taskQ;

	/* 管理者线程ID */
	pthread_t managerId;
	/* 工作的线程ID */
	pthread_t* threadIds;

	/* 最小线程数量 */
	int minNum;
	/* 最大线程数量 */
	int maxNum;
	/* 忙线程数量 */
	int busyNum;
	/* 存活线程数量 */
	int aliveNum;
	/* 待销毁线程数量 */
	int exitNum;
	/* 线程增减静态常量 */
	static const int NUMBER = 2;

	/* 锁整个线程池 */
	pthread_mutex_t mutexPool;
	/* 任务队列是不是空了 */
	pthread_cond_t notEmpty;

	/* 是否销毁线程池 */
	bool shutdown;

	/* 工作的线程调用函数 */
	static void* worker(void* arg);
	/* 管理者线程调用函数 */
	static void* manager(void* arg);
	/* 单个线程退出 */
	void threadExit();

};

