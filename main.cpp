//
//  main.cpp
//  
//
//  Created by apple on 8/12/21.
//


//信号处理函数
//自定义信号处理函数，创建sigaction结构体变量，设置信号函数
void sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno=errno;
    int msg=sig;
    
    //将信号从管道写端写入，传输字符类型，而非整型,通知主循环
    send(pipedfd[1],(char *)&msg,1,0);
    
    //将原来的errno赋值为当前的errno
    errno=save_errno;
}


//信号处理函数中仅仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响。
//设置信号处理函数
//项目中设置信号函数，仅关注SIGTERM和SIGALRM两个信号
void  addsig(int sig,void (handler)(int),bool restart=true)
{
    //创建sigaction结构体变量
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    
    //信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler=handler;
    if(restart) sa.sa_flags|=SA_RESTART;
    //将所有信号添加到信号集中
    sigfillset(&sa.sa_mask);
    
    //执行sigaction函数
    assert(sigaction(sig,&sa,NULL)!=-1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void timer_handler()
{
    timer_lst.tick();
    alarm(TIMESLOT);
}

//创建管道套接字
ret=socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
assert(ret!=0);

//设置管道写端为非阻塞，如果是阻塞的那么会增加信号处理函数的执行时间。
setnonblocking(pipefd[1]);

//设置管道读端为ET非阻塞
addfd(epollfd,pipefd[0],false);

//传递给主循环的信号值，这里只关注SIGALRM和SIGTERM
addsig(SIGALRM,sig_handler,false);
addsig(SIGTERM,sig_handler,false);

//循环条件
bool stop_server=false;

//超时标志
bool timeout=false;

//每隔TIMESLOT时间触发SIGALRM信号
alarm(TIMESLOT);

while(!stop_server)
{
    //监测发生事件的文件描述符
    int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
    if(number<0 && errno!=EINTR)
    {
        break;
    }
    
    //轮询文件描述符
    for(int i=0;i<number;i++)
    {
        int sockfd=events[i].data.fd;
        
        //管道读端对应文件描述符发生读事件
        if((sockfd==pipefd[0]) && (events[i].events & EPOLLIN))
        {
            int sig;
            char signals[1024];
            
            //从管道读端读出信号值，成功返回字节数，失败返回-1
            //正常情况下，这里的ret返回值总是1，只有14和15 两个ASCII码对应的字符
            ret=recv(pipefd[0],signals,sizeof(signals),0);
            if(ret==-1)
            {
                //handle the error
                continue;
            }
            else if(ret==0)
            {
                continue;
            }
            else
            {
                //处理信号值对应的逻辑
                for(int i=0;i<ret;i++)
                {
                    //这里面是字符
                    switch(signals[i])
                    {
                        case SIGALRM:
                        {
                            timeout=true;
                            break;
                        }
                        case SIGTERM:
                        {
                            stop_server=true;
                        }
                    }
                }
            }
        }
    }
}





