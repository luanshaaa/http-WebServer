//
//  sql_connection_pool.h
//  
//
//  Created by apple on 8/18/21.
//

#ifndef sql_connection_pool_h
#define sql_connection_pool_h

class connection_pool
{
public:
    //局部静态变量单例模式
    static connection_pool *GetInstance();
    
private:
    connection_pool();
    ~connection_pool();
}

connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}


class conenctionRAII{
    
public:
    //双指针对MYSQL *con修改
    connectionRAII(MYSQL **con,connection_pool *connPool)
    ~connectionRAII;
    
private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};
#endif /* sql_connection_pool_h */
