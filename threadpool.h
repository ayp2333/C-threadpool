#pragma once

typedef struct ThreadPool ThreadPool;
//�����̳߳ز���ʼ��
ThreadPool* threadPoolCreate(int min, int max, int queueSize);
//�����̳߳�
int threadPoolDestroy(ThreadPool* pool);
//���̳߳���������
void threadPoolAdd(ThreadPool* pool,void(*func)(void*),void* arg);
//��ȡ�̳߳��й������̸߳���
int threadPoolBusyNum(ThreadPool* pool);
//��ȡ�̳߳��л��ŵ��̸߳���
int threadPoolAliveNum(ThreadPool* pool);
////////////////////
void* worker(void* arg);
void* manager(void* arg);
void* threadExit(ThreadPool* pool);