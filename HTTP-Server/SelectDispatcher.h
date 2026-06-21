#pragma once

#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"
#include<string>
#include <sys/select.h>
using namespace std;

class SelectDispatcher : public Dispatcher
{
public:
	SelectDispatcher(EventLoop* evLoop);
	~SelectDispatcher();
	//添加
	int add() override;
	//删除
	int remove() override;
	//修改
	int modify() override;
	//事件监测
	int dispatch(int time_out = 2) override;
private:
	void setFdSet();
	void clearFdSet();
private:
	fd_set m_readfds;
	fd_set m_writefds;
	const int m_MAX_SIZE = 1024;

};