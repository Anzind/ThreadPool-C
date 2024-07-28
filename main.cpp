#include "ThreadPool.h"
#include "ThreadPool.cpp"

#include <iostream>
#include <unistd.h>

void taskFunc(void* arg)
{
	int num = *(int*)arg;
	std::cout << "thread " << std::to_string(pthread_self()) << " is working, number = " << num << std::endl;
	sleep(1);
}

int main()
{
	// 创建线程池
	ThreadPool<int>* pool = new ThreadPool<int>(3, 10);
	for (int i = 0; i < 100; ++i) {
		int* num = new int(i + 100);
		pool->addTask(Task<int>(taskFunc, num));
	}

	sleep(20);

	delete pool;
	return 0;
}