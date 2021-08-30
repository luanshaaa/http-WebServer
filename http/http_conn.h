//
//  httpconnect.h
//  
//
//  Created by apple on 2021/7/21.
//

#ifndef http_conn_h
#define http_conn_h
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
class http_conn
{
public:
    //设置读取文件的名称m_real_file大小
    static const int FILENAME_LEN = 200;
    //设置读缓冲区m_read_buf的大小
    static const int READ_BUFFER_SIZE = 2048;
    //设置写缓冲区m_write_buf的大小
    static const int WRITE_BUFFER_SIZE = 1024;
    //报文的请求方法，本项目只用到了GET和POST
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    //主状态机的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    //报文解析的结果
    enum HTTP_CODE
    {
        NO_REQUEST,//请求不完整，需要继续读取请求报文数据
        GET_REQUEST,//获得了完整的HTTP请求
        BAD_REQUEST,//HTTP请求报文有语法错误
        NO_RESOURCE,//请求资源不存在
        FORBIDDEN_REQUEST,//请求资源禁止访问，没有读取权限
        FILE_REQUEST,//请求资源可以正常访问
        INTERNAL_ERROR,//服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
        CLOSED_CONNECTION
    };
    //从状态机的状态,行的读取状态
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //初始化套接字地址，函数内部会调用私有方法init
    void init(int sockfd, const sockaddr_in &addr);
    //关闭http连接
    void close_conn(bool real_close = true);
    void process();//处理http请求
    //读取浏览器端发来的全部数据
    bool read_once();
    //响应报文写入函数
    bool write();
    //socket地址
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    //同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);

private:
    //初始化连接
    void init();
    //从m_read_buf读取，并处理请求报文
    HTTP_CODE process_read();
    //向m_write_buf写入响应报文数据
    bool process_write(HTTP_CODE ret);
    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char *text);
    //主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers(char *text);
    //主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char *text);
    //生成响应报文
    HTTP_CODE do_request();
    
    //m_start_line是已经解析过的字符
    //get_line用于将指针向后偏移，指向未处理的字符
    char *get_line() { return m_read_buf + m_start_line; };
    
    //从状态机读取一行，用于分析是请求报文的哪一部分
    LINE_STATUS parse_line();
    
    void unmap();
    
    //根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
//所有的socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的
    static int m_epollfd;
    //统计用户数量
    static int m_user_count;
    MYSQL *mysql;

private:
    //该http连接的socket和对方的socket地址
    int m_sockfd;
    sockaddr_in m_address;
    //读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
    /*标识读缓冲中已经读入的客户数据的最后一个字节的下一个位置*/
    int m_read_idx;
    /*当前正在分析的字符在读缓冲区中的位置*/
    int m_checked_idx;
    /*当前正在解析的行的起始位置*/
    int m_start_line;
    //存储发出的响应报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];
    /*写缓冲区中待发送的字节数*/
    int m_write_idx;
    
    //主状态机的状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;
    
    //一下为解析请求报文中对应的六个变量
//存储读取文件的名称，客户请求的目标文件的完整路径，其内容等于doc_root+m_url，doc_root是网站根目录
    char m_real_file[FILENAME_LEN];
    /*客户请求的目标文件的文件名*/
    char *m_url;
    /*HTTP协议版本号，我们仅支持HTTP/1.1*/
    char *m_version;
    /*主机名*/
    char *m_host;
    /*HTTP请求的消息体的长度*/
    int m_content_length;
    /*HTTP请求是否要求保持连接*/
    bool m_linger;
    
    //读取服务器上的文件地址
    /*客户请求的目标文件被mmap到内存中的起始位置*/
    char *m_file_address;
    /*目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取
    文件大小等信息*/
    struct stat m_file_stat;
    /*我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被
    写内存块的数量*/
    struct iovec m_iv[2];
    int m_iv_count;
    //是否启用的POST
    int cgi;
    //存储请求头数据
    char *m_string;
    //剩余发送字节数
    int bytes_to_send;
    //已发送字节数
    int bytes_have_send;
};

#endif /* http_conn_h */
