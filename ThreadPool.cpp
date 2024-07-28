#include "ThreadPool.h"

#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>

template<typename T>
ThreadPool<T>::ThreadPool(int min, int max) {

	do {
		/* 实例化任务队列 */
		taskQ = new TaskQueue<T>;
		if (!taskQ) {
			std::cout << "new TaskQueue fail..." << std::endl;
			break;
		}
		/* 实例化工作线程 */
		threadIds = new pthread_t[max];
		if (!threadIds) {
			std::cout << "new threadIds fail..." << std::endl;
			break;
		}
		memset(threadIds, 0, sizeof(pthread_t) * max);
		minNum = min;
		maxNum = max;
		busyNum = 0;
		aliveNum = min;    // 和最小个数相等
		exitNum = 0;

		if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
			pthread_cond_init(&notEmpty, NULL) != 0) {
			std::cout << "mutex or condition init fail..." << std::endl;
			break;
		}

		shutdown = false;

		// 创建线程
		pthread_create(&managerId, NULL, manager, this);
		for (int i = 0; i < min; ++i)
		{
			pthread_create(&threadIds[i], NULL, worker, this);
		}
		return;
	} while (0);

	// 释放资源
	if (threadIds) delete[] threadIds;
	if (taskQ) delete taskQ;

	return;
}

template<typename T>
ThreadPool<T>::~ThreadPool() {
	// 关闭线程池
	shutdown = true;
	// 阻塞回收管理者线程
	pthread_join(managerId, NULL);
	// 唤醒阻塞的消费者线程
	for (int i = 0; i < aliveNum; ++i)
	{
		pthread_cond_signal(&notEmpty);
	}

	// 释放堆内存
	if (taskQ) delete taskQ;
	if (threadIds) delete[] threadIds;

	pthread_mutex_destroy(&mutexPool);
	pthread_cond_destroy(&notEmpty);

	std::cout << "ThreadPool Exit, thanks!!" << std::endl;
}

template<typename T>
void ThreadPool<T>::addTask(Task<T> task) {
	if (shutdown) return;
	// 添加任务
	taskQ->addTask(task);

	pthread_cond_signal(&notEmpty);
}

template<typename T>
int ThreadPool<T>::getBusyNum() {
	pthread_mutex_lock(&mutexPool);
	int busyNum = this->busyNum;
	pthread_mutex_unlock(&mutexPool);
	return busyNum;
}

template<typename T>
int ThreadPool<T>::getAliveNum() {
	pthread_mutex_lock(&mutexPool);
	int aliveNum = this->aliveNum;
	pthread_mutex_unlock(&mutexPool);
	return aliveNum;
}

template<typename T>
void* ThreadPool<T>::worker(void* arg) {
	ThreadPool<T>* pool = static_cast<ThreadPool<T>*>(arg);

	while (true)
	{
		pthread_mutex_lock(&pool->mutexPool);
		// 当前任务队列是否为空
		while (pool->taskQ->taskNumber() == 0 && !pool->shutdown)
		{
			// 阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			// 判断是不是要销毁线程
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				if (pool->aliveNum > pool->minNum)
				{
					pool->aliveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					pool->threadExit();
				}
			}
		}

		// 判断线程池是否被关闭了
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			pool->threadExit();
		}

		// 从任务队列中取出一个任务
		Task<T> task = pool->taskQ->takeTask();

		// 解锁
		pool->busyNum++;
		std::cout << "thread " << std::to_string(pthread_self()) << " start working..." << std::endl;
		pthread_mutex_unlock(&pool->mutexPool);


		task.function(task.arg);
		delete task.arg;
		task.arg = nullptr;

		pthread_mutex_lock(&pool->mutexPool);
		std::cout << "thread " << std::to_string(pthread_self()) << " end working..." << std::endl;
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexPool);
	}
	return NULL;
}

template<typename T>
void* ThreadPool<T>::manager(void* arg) {
	ThreadPool<T>* pool = static_cast<ThreadPool<T>*>(arg);
	while (!pool->shutdown) {
		// 每隔3s检测一次
		sleep(3);

		// 取出线程池中任务的数量和当前线程的数量、忙的线程的数量
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->taskQ->taskNumber();
		int aliveNum = pool->aliveNum;
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// 添加线程
		// 任务的个数>存活的线程个数 && 存活的线程数<最大线程数
		if (queueSize > aliveNum && aliveNum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER
				&& pool->aliveNum < pool->maxNum; ++i)
			{
				if (pool->threadIds[i] == 0)
				{
					pthread_create(&pool->threadIds[i], NULL, worker, pool);
					counter++;
					pool->aliveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}
		// 销毁线程
		// 忙的线程*2 < 存活的线程数 && 存活的线程>最小线程数
		if (busyNum * 2 < aliveNum && aliveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// 让工作的线程自杀
			for (int i = 0; i < NUMBER; ++i)
			{
				pthread_cond_signal(&pool->notEmpty);
			}
		}
	}
	return NULL;
}

template<typename T>
void ThreadPool<T>::threadExit() {
	pthread_t tid = pthread_self();
	for (int i = 0; i < maxNum; ++i)
	{
		if (threadIds[i] == tid)
		{
			threadIds[i] = 0;
			std::cout << "threadExit() called, " << std::to_string(tid) << " exiting..." << std::endl;
			break;
		}
	}
	pthread_exit(NULL);
}
