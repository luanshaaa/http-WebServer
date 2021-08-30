//
//  block_queue.h
//  
//
//  Created by apple on 8/15/21.
//

#ifndef block_queue_h
#define block_queue_h

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"
using namespace std;

//阻塞队列，封装了生产者-消费者模型
template<class T>
class block_queue
{
public:
    //初始化私有成员
    block_queue(itn max_size=1000)
    {
        if(max_size<=0)
        {
            exit(-1);
        }
        
        //构造函数创建循环数组
        m_max_size=max_size;
        m_array=new T[max_size];
        m_size=0;
        m_front=-1;
        m_back=-1;
        
        //创建互斥锁和条件变量
        m_mutex=new thread_mutex_t;
        m_cond=new pthread_cond_t;
        pthread_mutex_init(m_mutex,NULL);
        pthread_cond_init(m_cond,NULL);
    }
    
    //往队列添加元素，需要将所有使用队列的线程先唤醒
    //当有元素push进队列，相当于生产者生产了一个元素
    //若当前没有线程等待条件变量，则唤醒无意义
    bool push(const T &item)
    {
        pthread_mutex_lock(m_mutex);
        if(m_size>=m_max_size)
        {
            pthread_cond_broadcast(m_cond);
            pthread_mutex_unlock(m_mutex);
            return false;
        }
        
        //将新增数据放在循环数组的对应位置
        m_back=(m_back+1)%m_max_size;
        m_array[m_back]=item;
        m_size++;
        
        pthread_cond_broadcast(m_cond);
        pthread_mutex_unlock(m_mutex);
        return true;
    }
    
    //pop时，如果当前队列没有元素，将会等待条件变量
    bool pop(T &item)
    {
        pthread_mutex_lock(m_mutex);
        
        //多个消费者的时候,这里要是用while而不是if
        while(m_size<=0)
        {
            //当重新抢到互斥锁，pthread_cond_wait返回为0
            if(pthread_cond_wait(m_cond,m_mutex)!=0)
            {
                pthread_mutex_unlock(m_mutex);
                return false;
            }
        }
        
        //取出队列首元素
        m_front=(m_front+1)%m_max_size;
        item=m_array[m_front];
        m_size--;
        pthread_mutex_unlock(m_mutex);
        return true;
    }
    
    //增加了超时处理（未用到）
    //在pthread_cond_wait基础上增加了等待时间
    bool pop(T &item,int ms_timeout)
    {
        struct timespec t={0,0};
        struct timeval now={0,0};
        gettimeofday(&now,,NULL);
        pthread_mutex_lock(m_mutex);
        if(m_size<=0)
        {
            t.tv_sec=now.tv_sec+ms_timeout/1000;
            t.tv_nsec=(ms_timeout%1000)*1000;
            if(pthread_cond_timedwait(m_cond,m_mutex,&t)!=0)
            {
                pthread_mutex_unlock(m_mutex);
                return false;
            }
        }
        
        if(m_size<=0)
        {
            pthread_mutex_unlock(m_mutex);
            return false;
        }
        
        m_front=(m_front+1)%m_max_size;
        item=m_array[m_front];
        m_size--;
        pthread_mutex_unlock(m_mutex);
        return true;
    }
}
#endif /* block_queue_h */
