//
//  locker.h
//  
//
//  Created by apple on 2021/7/23.
//

#ifndef locker_h
#define locker_h

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//封装信号量的类
class sem
{
public:
  //创建并初始化信号量
    sem()
    {
        if(sem_init(&m_sem,0,0) != 0)//成功时返回0
        {
            /*构造函数没有返回值，可以通过抛出异常来报告错误*/
            throw std::exception();
        }
    }
    sem(int num)//创建并初始化信号量,指定信号量的值为num
    {
        if(sem_init(&m_sem,0,num)!=0)//第二个0表示当前进程局部信号量
        {
            throw std::exception();
        }
    }
    ~sem()//销毁信号量
    {
        sem_destroy(&m_sem);
    }
    bool wait()//等待信号量
    {
        return sem_wait(&m_sem) == 0;//以原子操作的方式将信号值减一，若m_sem为0，则sem_wait被阻塞，直到m_sem有非0值
    }
    bool post()//增加信号量
    {
        return sem_post(&m_sem) == 0;//以原子操作的方式将信号值加一，若m_sem大于0，则其他正在调用sem_wait的信号量的线程被唤醒
    }
private:
    sem_t m_sem;//信号量
};

//封装互斥锁的类
class locker
{
public:
    /*创建并初始化互斥锁*/
    locker()
    {
        if(pthread_mutex_init(&m_mutex,NULL)!=0)//NULL表示默认属性的互斥锁
        {
            throw std::exception();
        }
    }
    /*销毁互斥锁*/
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    /*加锁*/
    bool lock()
    {
        //以原子操作的方式给一个互斥锁加锁
        return pthread_mutex_lock(&m_mutex)==0;
    }
    /*释放互斥锁*/
    bool unlock()
    {
        //以原子操作的方式给一个互斥锁解锁
        return pthread_mutex_unlock(&m_mutex)==0;
    }
    pthread_mutex_t *get()//返回指向该互斥锁的指针
    {
        return &m_mutex;
    }
    
private:
    pthread_mutex_t m_mutex;//定义一个互斥锁
};

/*封装条件变量的类*/
class cond
{
public:
    /*创建并初始化条件变量*/
    cond()
    {
        if(pthread_cond_init(&m_cond,NULL)!=0)
        {
            /*构造函数中一旦出现问题，就应该立即释放已经成功分配了的资源*/
            //pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }
    /*销毁条件变量*/
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    /*等待条件变量*/
    bool wait(pthread_mutex_t *m_mutex)
    {
        int ret=0;
        //pthread_mutex_lock(&m_mutex);
        ret=pthread_cond_timedwait(&m_cond,m_mutex,&t);
        //pthread_mutex_unlock(&m_mutex);
        return ret==0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    /*唤醒一个等待条件变量的线程*/
    bool signal()
    {
        return pthread_cond_signal(&m_cond)==0;
    }
    //唤醒所有等待目标条件变量的线程
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond)==0;
    }
private:
    //static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;//条件变量
};
#endif /* locker_h */
