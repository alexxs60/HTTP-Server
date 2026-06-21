#pragma once
#include "Channel.h"
#include<string>
using namespace std;
 
class EventLoop;
class Dispatcher
{
public:
	Dispatcher(EventLoop* evLoop);
	virtual ~Dispatcher();
	//添加
	virtual int add();
	//删除
	virtual int remove();
	//修改
	virtual int modify();
	//事件监测
	virtual int dispatch(int time_out = 2);

	inline void setChannel(Channel* channel) { m_channel = channel; }
protected:
	string m_name=string();
	Channel* m_channel;
	EventLoop* m_evLoop;
};

