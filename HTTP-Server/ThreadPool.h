#pragma once
#include "EventLoop.h"
#include "WorkThread.h"
#include <cassert>
#include<stdlib.h>
#include<vector>
using namespace std;

class ThreadPool 
{
public:
	ThreadPool(EventLoop* mainLoop, int count);
	~ThreadPool();
	//启动线程池
	void running();
	//取出线程池中某个子线程的反应堆实例
	EventLoop* takeWorkEventLoop();
private:
	bool m_isStarted;
	int m_threadNum;
	int m_index;
	vector<WorkThread*> m_workThreads;
	//主反应堆模型
	EventLoop* m_mainLoop;
};

