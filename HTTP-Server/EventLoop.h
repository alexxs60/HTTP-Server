#pragma once
#include "Dispatcher.h"
#include "Channel.h"
#include "EpollDispatcher.h"
#include "PollDispatcher.h"
#include "SelectDispatcher.h"
#include <assert.h>
#include<string>
#include<thread>
#include<queue>
#include<map>
#include<mutex>
#include<sys/socket.h>
#include<sys/types.h>
#include <unistd.h> // 添加此头文件以修复E0020: 未定义标识符 "read"
#include<string.h>
using namespace std;
 
//处理channel的方式
enum class ElemType:char{ADD,DELETE,MODIFY};
//定义任务队列的节点
struct ChannelElement
{
	Channel* channel;
	ElemType type;
};

class Dispatcher;

class EventLoop
{
public:
	EventLoop();
	EventLoop(const string threadName);
	~EventLoop();
	//启动反应堆文件
	int run();
	//激活被处理的文件fd
	int eventActive(int fd, int event);
	//添加任务到队列
	int addTask(struct Channel* channel, ElemType type);
	//处理任务队列中的任务
	int processTaskQ();
	//处理dispatcher中的节点
	int add(Channel* channel);
	int remove(Channel* channel);
	int modify(Channel* channel);
	//释放channel
	int destroyChannel(Channel* channel);
	static int readLocalMessage(void* arg);
	int readMessage();
	inline thread::id getThreadId() { return m_threadId; }
private:
	void taskWakeup();
private:
	bool m_isQuit;
	//指向子类的实例
	Dispatcher* m_dispatcher;
	//任务队列
	queue<ChannelElement*>m_taskQ;
	//map
	map<int, Channel*> m_channelMap;
	//线程id，name，mutex
	thread::id m_threadId;
	string m_threadName;
	mutex m_mutex;
	int m_socketpairfd[2]; // 用于线程间通信的socketpair
};
