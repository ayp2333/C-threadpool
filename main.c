#include <stdio.h>
#include "threadpool.h"
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

void testFunc(void *arg)
{
    int num = *(int*)arg;
    printf("当前线程ID = %ld ,number=%d \n", pthread_self(), num);
    sleep(1);

}
int main()
{
    ThreadPool *pool = threadPoolCreate(3, 10, 100);
    for (int i = 0; i < 100; i++)
    {
        int* num = (int*)malloc(sizeof(int));
        *num = i + 100;

        threadPoolAdd(pool, testFunc, num);

    }
    sleep(30);
    threadPoolDestroy(pool);
    return 0;
}