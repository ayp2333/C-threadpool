#include "threadpool.h"
#include <pthread.h>
#include<stdlib.h>
#include <stdio.h>
#include<string.h>
#include<unistd.h>

const int NUMBER = 2;
typedef struct Task
{
	void (*function)(void* arg);
	void* arg;
}Task;

struct ThreadPool
{
	//任务队列
	Task* taskQ;
	int queueCapacity; // 容量
	int queueSize; //当前任务个数
	int queueFront; //队头 -> 取数据
	int queueRear; //队尾

	pthread_t managerID;
	pthread_t* threadIDs;
	int minNum;  //最小线程个数
	int maxNum;  // 最大线程个数
	int busyNum;   //忙线程的个数
	int liveNum;  //存活的线程
	int exitNum;  //需要杀死线程的个数
	pthread_mutex_t mutexPool; // 整个线程池的锁
	pthread_mutex_t mutexBusy; // busyNum的锁
	
	pthread_cond_t notFull; // 队列是否满 
	pthread_cond_t notEmpty; // 队列是否空

	int shutdown;  // 是否要杀死线程,销毁为 1 

};

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do
	{
		if (pool == NULL)
		{
			printf("malloc threadpool fail ... \n");
			break;
		}
		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->threadIDs == NULL)
		{
			printf("malloc threadIDsfail ...\n");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min; //按照最小个数来初始化
		pool->exitNum = 0;

		//初始化互斥锁和条件变量
		if (pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0)
		{
			printf("mutex or cond init fail ... \n");
			break;
		}

		//任务队列初始化

		pool->taskQ = (Task *)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		pool->shutdown = 0;


		//创建线程

		pthread_create(&pool->managerID, NULL, manager, pool);

		for (int i = 0; i < min; i++)
		{
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}
		return pool;
	} while (0);

	//释放资源
	if (pool&&pool->threadIDs) free(pool->threadIDs);
	if (pool && pool->taskQ) free(pool->taskQ);
	if (pool) free(pool);

	return NULL;
}
int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL)
	{
		return -1;
	}
	pool->shutdown = 1;
	pthread_join(pool->managerID, NULL);
	//唤醒阻塞的消费者
	for (int i = 0; i < pool->liveNum; i++)
	{
		pthread_cond_signal(&pool->notEmpty);
	}
	//释放内存

	if (pool->taskQ)
	{
		free(pool->taskQ);
	}

	if (pool->threadIDs)
	{
		free(pool->threadIDs);
	}

	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);
	free(pool);
	pool = NULL;

	return 0;
}
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexPool);
	while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
	{
		//阻塞生产者线程
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);

	}
	if (pool->shutdown)
	{
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	//添加任务
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;
	pthread_cond_signal(&pool->notEmpty);

	pthread_mutex_unlock(&pool->mutexPool);
}
int threadPoolBusyNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}
int threadPoolAliveNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexPool);
	int aliveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);
	return aliveNum;
}
	
void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;

	while (1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		//当前任务队列是否为空
		while (pool->queueSize == 0 && !pool->shutdown)
		{
			//阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
			if (pool->exitNum>0)
			{
				pool->exitNum--;
				if (pool->liveNum>pool->minNum)
				{
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}

		}
		//判断线程池是否关闭
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}
		//从任务队列中取出一个任务
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;
		//移动头结点
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;
		//解锁
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);

		printf("thread %ld strat working ... \n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum ++;
		pthread_mutex_unlock(&pool->mutexBusy);

		task.function(task.arg);
		free(task.arg);
		task.arg = NULL;

		printf("thread %ld end working ... \n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum --;
		pthread_mutex_unlock(&pool->mutexBusy);
		
	}
	return NULL;
}

void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutdown)
	{
		sleep(3);

		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;		
		pthread_mutex_unlock(&pool->mutexPool);

		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum=pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		//添加线程
		//判断逻辑 已有任务 > 存活线程 && 存活线程<最大线程数
		
		if (queueSize>liveNum&&liveNum<pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			int coutner = 0;
			for (int  i = 0; i < pool->maxNum&&coutner<NUMBER
				&&liveNum<pool->maxNum; i++)
			{
				if (pool->threadIDs[i]==0)
				{
					pthread_create(&pool->threadIDs[i],NULL,worker,pool);
					coutner++;
					pool->liveNum++;	
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}
		// 销毁线程
		//忙的线程*2 <存活的线程数 && 存活的线程>最小线程数
		if (busyNum*2<liveNum && liveNum>pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			//让工作的线程自杀
			for (int  i = 0; i < NUMBER; i++)
			{
				pthread_cond_signal(&pool->notEmpty);
			}
		}

	}
	return NULL;
}

void* threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int  i = 0; i < pool->maxNum; i++)
	{
		if (pool->threadIDs[i] == tid)
		{
			pool->threadIDs[i] = 0;
			printf("threadExit () called ,%ld exiting ...\n", tid);
			break;
		}
	}
	pthread_exit(NULL);
}

