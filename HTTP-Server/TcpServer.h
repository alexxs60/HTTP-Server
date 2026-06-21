#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"
#include <arpa/inet.h>
#include<stdio.h>
#include<stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "TcpConnection.h"
#include "Log.h"

class TcpServer 
{
public:
	TcpServer(unsigned short port, int threadNum);
	~TcpServer();
	//初始化监听
	void setListener();
	//启动服务器
	void starting();
	static int acceptConnection(void* arg);
	int accept_Connection();
private:
	int m_threadNum;
	EventLoop* m_mainLoop;
	ThreadPool* m_threadPool;
	int m_lfd;
	unsigned  short m_port;
};

