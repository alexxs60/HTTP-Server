#pragma once
#include<thread>
#include<mutex>
#include<string>
#include<stdio.h>
#include "EventLoop.h"
#include <condition_variable>
using namespace std;

class WorkThread {
public:
	WorkThread(int index);
	~WorkThread();
	//启动工作线程
	void start();
	inline EventLoop* getEventLoop() {return m_evLoop;}

private:
	void threadFunc();
private:
	thread* m_thread; //保存线程的实例
	thread::id m_threadId;
	string m_name;
	mutex m_mutex;
	condition_variable m_cond;
	EventLoop* m_evLoop;
};




