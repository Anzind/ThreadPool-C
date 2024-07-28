/**
	任务队列类
*/

#pragma once
#include <queue>
#include <pthread.h>

using callback = void (*)(void* arg);

template<typename T>
struct Task {
	/* 无参构造 */
	Task() :function(nullptr), arg(nullptr) {}
	/* 有参构造 */
	Task(callback fun, void* arg) : function(fun), arg((T*)arg) {}

	callback function;
	T* arg;
};

template<typename T>
class TaskQueue {

public:
	/* 无参构造 */
	TaskQueue();
	/* 析构函数 */
	~TaskQueue();

	/* 添加任务 */
	void addTask(Task<T> task);
	void addTask(callback fun, void* arg);
	/* 取出任务 */
	Task<T> takeTask();

	/* 获取当前任务的个数 */
	inline size_t taskNumber() {
		return m_taskQ.size();
	}


private:
	/* 任务队列 */
	std::queue<Task<T>> m_taskQ;

	/* 任务队列互斥锁 */
	pthread_mutex_t m_mutex;

};

