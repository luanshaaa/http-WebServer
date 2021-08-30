//
//  sql_connection_pool.cpp
//  
//
//  Created by apple on 8/18/21.
//

#include <stdio.h>

connection_pool::connection_pool()
{
    this->CurConn=0;
    this->FreeConn=0;
}

//RAII机制销毁连接池
connection_pool::~connection_pool()
{
    DestroyPool();
}

//构造初始化
void connection_pool::init(string url, string User,string PassWord, string DBName, int Port, unsigned int MaxConn)
{
    //初始化数据库信息
    this->url=url;
    this->Port=Port;
    this->User=User;
    this->PassWord=PassWord;
    this->DatabaseName=DBName;
    
    //创建MaxConn条数据库连接
    for(int i=0;i<MaxConn;i++)
    {
        MYSQL *con=NULL;
        con=mysql_init(con);
        
        if(con==NULL)
        {
            cout<<"Error:"<<mysql_error(con);
            exit(1);
        }
        con=mysql_real_connect(con,url.c_str(),User.c_str(),PassWord.c_str(),DBName.c_str(),Port,NULL,0);
    
        if(con==NULL)
        {
            cout<<"Error: "<<mysql_error(con);
            exit(1);
        }
        
        //更新连接池和空闲连接数量
        connList.push_back(con);
        ++FreeConn;
    }
    
    //将信号量初始化为最大连接次数
    reserve=sem(FreeConn);
    
    this->MaxConn=FreeConn;
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con=NULL;
    
    if(0==connlist.size())
        return NULL;
    
    //取出连接，信号量原子减1，为0则等待
    reserve.wait();
    
    lock.lock();
    
    con=connList.front();
    connList.pop_front();
    
    //这里的两个变量没有用到
    --FreeConn;
    ++CurConn;
    
    lock.unlock();
    return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if(con==NULL)
        return false;
    
    lock.lock();
    
    connList.push_back(con);
    ++FreeConn;
    --CurConn;
    
    lock.unlock();
    
    //释放连接原子加1
    reserve.post();
    return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{
    lock.lock();
    if(connList.size()>0)
    {
        //通过迭代器遍历，关闭数据库连接
        list<MYSQL *>::iterator it;
        for(it=connList.begin();it!=connList.end();++it)
        {
            MYSQL *con=*it;
            mysql_close(con);
        }
        
        CurConn=0;
        FreeConn=0;
        
        //清空list
        connList.clear();
        lock.unlock();
    }
    
    lock.unlock();
}

connectionRAII::connectionRAII(MYSQL **SQL,connection_pool *connPool)
{
    *SQL=connPool->GetConnection();
    
    conRAII=*SQL;
    poolRAII=connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}
