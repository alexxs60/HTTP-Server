#include "ThreadPool.h"

ThreadPool::ThreadPool(EventLoop* mainLoop, int count)
{
	m_index = 0;
	m_threadNum = count;
	m_mainLoop = mainLoop;
	m_isStarted = false;
	m_workThreads.clear();
}

ThreadPool::~ThreadPool() 
{
	for(auto workThread : m_workThreads)
	{
		delete workThread;
	}
}
void ThreadPool::running() {
	assert(!m_isStarted);
	if (m_mainLoop->getThreadId() != this_thread::get_id())
	{
		exit(0);
	}
	m_isStarted = true;
	if (m_threadNum>0)
	{
		for (int i = 0; i < m_threadNum; i++)
		{
			WorkThread* workThread = new WorkThread(i);
			workThread->start();  //当条件变量阻塞时，主线程会阻塞在这里
			m_workThreads.push_back(workThread);
		}
	}
}
EventLoop* ThreadPool::takeWorkEventLoop() 
{
	assert(m_isStarted);
	if (m_mainLoop->getThreadId() != this_thread::get_id())
	{
		exit(0);
	}
	EventLoop* evLoop = m_mainLoop;
	if (m_threadNum > 0)
	{
		evLoop = m_workThreads[m_index]->getEventLoop();
		m_index = (m_index + 1) % m_threadNum;
	}
	return evLoop;
}
