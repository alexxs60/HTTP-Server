#pragma once

#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"
#include<string>
#include<sys/epoll.h>
using namespace std;

class EpollDispatcher : public Dispatcher
{
public:
	EpollDispatcher(EventLoop* evLoop);
	~EpollDispatcher();
	//添加
	int add() override;
	//删除
	int remove() override;
	//修改
	int modify() override;
	//事件监测
	int dispatch(int time_out = 2) override;
private:
	int epollCtl(int op);
private:
	int m_epfd;
	struct epoll_event* m_events;
	const int m_MAX_EVENTS = 520;
};

