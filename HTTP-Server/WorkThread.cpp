#include "WorkThread.h"
#include "ThreadPool.h"

WorkThread::WorkThread(int index)
{
	m_evLoop = nullptr;
	m_thread = nullptr;
	m_threadId = thread::id();
	m_name = "SubThread-" + to_string(index);
}

WorkThread::~WorkThread()
{
	if(m_thread != nullptr)
	{
		m_thread->join();
		delete m_thread;
		m_thread = nullptr;
	}
}

void WorkThread::start()  //这个函数属于主线程
{
	m_thread = new thread(&WorkThread::threadFunc, this); //?
	unique_lock<mutex> locker(m_mutex);
	while (m_evLoop == nullptr)
	{
		m_cond.wait(locker);  //阻塞的是主线程还是子线程？主线程在等待子线程创建EventLoop实例后通知它继续执行
		//do something
	}
}

void WorkThread::threadFunc()   //任务函数属于子线程
{
	m_mutex.lock();
	m_evLoop = new EventLoop(m_name);
	m_mutex.unlock();
	m_cond.notify_one();
	m_evLoop->run();

}
