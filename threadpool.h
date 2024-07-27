#ifndef _THREADPOOL_H__
#define _THREADPOOL_H__

typedef struct ThreadPool ThreadPool;
/* 创建线程池并初始化 */
ThreadPool* threadPoolCreate(int min, int max, int queueSize);

/* 销毁线程池 */
int threadPoolDestroy(ThreadPool* pool);

/* 给线程池添加任务 */
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);

/* 获取线程池中工作的线程个数 */
int threadPoolBusyNum(ThreadPool* pool);

/* 获取线程池中活着的线程个数 */
int threadPoolAliveNum(ThreadPool* pool);

/* 线程退出 */
void threadExit(ThreadPool* pool);

////////////////////////
/* 线程使用的函数 */
void* worker(void* arg);
void* manager(void* arg);
#endif /* _THREADPOOL_H__ */