//
//  log.h
//  
//
//  Created by apple on 8/15/21.
//

#ifndef log_h
#define log_h

class Log
{
public:
    //c++11使用局部变量懒汉不用加锁,创建日志实例
    static Log *get_instance()
    {
        static Log instances;
        return &instances;
    }
    
    //可选择的参数有日志文件，日志缓冲区的大小，最大行数以及最大日志条队列
    bool init(const char *file_name,int log_bud_size=8192,int split_lines=5000000,int max_queue_size=0);
    //异步写日志公有方法，调用私有方法async_write_log
    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }
    
    //将输出内容按照标准格式整理
    void write_log(int level,const char *format,...);
    
    //强制刷新缓冲区
    void flush(void);
    
private:
    Log();
    virtual ~Log();
    
    //异步写日志方法
    void *async_write_log()
    {
        string single_log;
        
        //从阻塞队列中取出一条日志内容，写入文件
        while(m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(),m_fp);
            m_mutex.unlock();
        }
    }
    
    
private:
    char dir_name[128];  //路径名
    char log_nme[128];   //log文件名
    int m_split_lines;   //日志最大行数
    int m_log_buf_size;  //日志缓冲区大小
    long long m_count;   //日志行数纪录
    int m_today;         //按天分文件，记录当前时间是哪一天
    FILE *m_fp;          //打开log的文件指针
    char *m_buf;         //要输出的内容
    block_queue<string> *m_log_queue;//阻塞队列
    bool m_is_async;     //是否同步标志位
    locker m_mutex;      //同步类
}；

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif /* log_h */
