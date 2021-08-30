//
//  threadpool.h
//  
//
//  Created by apple on 2021/7/24.
//

#ifndef threadpool_h
#define threadpool_h
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

//线程池
template <typename T>
class threadpool
{
public:
    //thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的，等待处理的请求的数量
    threadpool(connection_pool *connpool, int thread_number=8,int max_request=10000);
    ~threadpool();
    bool append(T *request);
    
private:
    //工作线程运行的函数，它不断从工作队列中取出任务并执行之
    static void *worker(void *arg);
    void run();
    
private:
    int m_thread_number;//线程池中的线程数
    int m_max_requests;//请求队列只能够允许的最大请求数
    pthread_t *m_threads;//描述线程池的数组，其大小为m_thread_number;
    std::list<T*> m_workqueue;//请求队列
    locker m_queuelocker;//保护请求队列的互斥锁
    sem m_queuestat//是否有d任务需要处理
    bool m_stop;//是否结束线程
    connection_pool *m_connPool;//数据库
};

//定义构造函数
template<typename T>
threadpool<T>::threadpool(connection_pool *connPool, int thread_number,int max_request):m_thread_number(thread_number),m_max_requests(max_request),m_stop(false),m_threads(NULL),m_connPool(connpool)
{
    if(thread_number<=0 || max_requests<=0)
        throw std::exception();
    
    //线程id初始化
    m_threads=new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    /*创建thread_number个线程，并将它们都设置为脱离线程*/
    for(int i=0;i<thread_number;i++)
    {
        //printf("create the %dth thread\n",i);
        if(pthread_create(m_threads+i,NULL,worker,this)!=0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]))//设置为脱离线程，脱离与其他线程的同步
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

//析构函数
template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop=true;//停止线程
}


//通过list容器创建请求队列，向队列中添加时，通过互斥锁保证线程安全，添加完成后通过信号量提醒有任务要处理，最后注意线程同步。
template<typename T>
bool threadpool<T>::append(T* request)
{
    m_queuelocker.lock();//给请求队列加锁
    //根据硬件，预先设置请求队列的最大值
    if(m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.ulock();
        return false;
    }
    //添加任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    //信号量提醒有任务要处理
    m_queuestat.post();//以原子操作的方式将信号量的值加1
    return true;
}

//线程处理函数，内部访问私有成员函数run，完成线程处理要求
template<typename T>
void* threadpool<T>::worker(void *arg)
{
    //将参数强转为线程池类，调用成员方法
    threadpool *pool=(threadpool*)arg;
    pool->run();
    return pool;
}


//执行任务，主要实现，工作线程从请求队列中取出某个任务进行处理，注意线程同步。
template<typename T>
void threadpool<T>::run()
{
    while(!m_stop)
    {
        //信号量等待
        m_queuestat.wait();
        //唤醒后先给请求队列互斥锁加锁
        m_queuelocker.lock();
        if(m_workqueue.empty())//如果没有请求，就继续等待信号量
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request=m_workqueue.front();//找到第一个请求
        m_workqueue.pop_front();//从任务中删除该请求
        m_queuelocker.unlock();//互斥锁解锁
        if(!request) continue;
        //从连接池中取出一个数据库连接
        connectionRAII mysqlcon(&request->mysql,m_connPool);
        //process(模板类中的方法,这里是http类)进行处理
        request->process();
        //将数据库连接放回连接池
        m_connPool->ReleaseConnection(request->mysql);
    }
}

#endif /* threadpool_h */
