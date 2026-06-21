#include "EventLoop.h"


EventLoop::EventLoop() : EventLoop(string())//委托构造函数，调用另一个构造函数进行初始化
{
}

EventLoop::EventLoop(const string threadName) 
{
	m_isQuit = true;//默认为true
	m_threadId = this_thread::get_id();	
	m_threadName = threadName == string() ? "MainThread" : threadName;
	m_dispatcher = new EpollDispatcher(this); // 默认使用EpollDispatcher，可以根据需要修改为其他调度器
	m_channelMap.clear();
	int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_socketpairfd);
	if (ret == -1) {
		perror("socketpair");
		exit(0);
	}
#if 0
	//指定socketpairfd[0]为写端，socketpairfd[1]为读端
	Channel* channel = new Channel(m_socketpairfd[1], FDEvent::READ_EVENT, readLocalMessage, nullptr, nullptr, this);
#else
	//绑定 -bind
	auto obj = bind(&EventLoop::readMessage, this);//把非静态成员函数和对象绑定成可调用对象
	Channel* channel = new Channel(m_socketpairfd[1], FDEvent::READ_EVENT, obj, nullptr, nullptr, nullptr);
#endif
	//channel添加到任务队列
	addTask(channel, ElemType::ADD);
}

EventLoop::~EventLoop()
{
}

int EventLoop::run()
{	
	m_isQuit = false;
	if (m_threadId != this_thread::get_id()) {
		return -1;
	}
	while (!m_isQuit) {
		m_dispatcher->dispatch();
		processTaskQ();
	}
	return 0;
}

int EventLoop::eventActive(int fd, int event)
{
	if (fd < 0) {
		return -1;
	}
	//取出channel
	auto it = m_channelMap.find(fd);
	if (it == m_channelMap.end() || it->second == nullptr) {
		return -1;
	}
	Channel* channel = it->second;
	assert(channel->getSocket() == fd);
	if (event & static_cast<int>(FDEvent::READ_EVENT) && channel->readCallback) {
		channel->readCallback(const_cast<void*>(channel->getArg()));
		//防止读回调删连接后继续写回调
		it = m_channelMap.find(fd);
		if (it == m_channelMap.end() || it->second == nullptr) {
			return 0;
		}
		channel = it->second;
	}
	if (event & static_cast<int>(FDEvent::WRITE_EVENT) && channel->writeCallback) {
		channel->writeCallback(const_cast<void*>(channel->getArg()));
	}
	return 0; 
}

int EventLoop::addTask(Channel* channel, ElemType type)
{
	//加锁，保护共享资源
	m_mutex.lock();
	//创建任务节点
	ChannelElement* node = new ChannelElement;
	node->channel = channel;
	node->type = type;
	m_taskQ.push(node);
	m_mutex.unlock();
	//处理节点
	/*细节：
		1.对于链表节点的添加：可能是当前线程也可能是其他线程（主线程）
			1）修改fd的事件，当前子线程发起，当前子线程处理
			2）添加fd，添加任务节点的操作是由主线程发起的
		2.不能让主线程处理任务队列，需要由当前的子线程处理*/
	if (m_threadId == this_thread::get_id()) {
		//当前子线程
		processTaskQ();
	}
	else {
		//主线程 -- 告诉子线程处理任务队列中的任务
		//1.子线程在工作 2.子线程被阻塞了：select，poll，epoll
		taskWakeup();
	}
	return 0;
}

int EventLoop::processTaskQ()
{
	while (!m_taskQ.empty())
	{
		m_mutex.lock();
		if (m_taskQ.empty()) {
			m_mutex.unlock();
			break;
		}
		ChannelElement* node = m_taskQ.front();
		m_taskQ.pop();
		m_mutex.unlock();
		Channel* channel = node->channel;
		if (node->type == ElemType::ADD) {
			//添加
			add(channel);
		}
		else if (node->type == ElemType::DELETE) {
			//删除
			remove(channel);
		}
		else if (node->type == ElemType::MODIFY) {
			//修改
			modify(channel);
		}
		delete node;
	}

	return 0;
}

int EventLoop::add(Channel* channel)
{
	int fd = channel->getSocket();
	//找到fd对应的数组元素位置，并存储
	if (m_channelMap.find(fd) == m_channelMap.end())
	{
		m_channelMap.insert(make_pair(fd, channel));
		m_dispatcher->setChannel(channel);
		int ret = m_dispatcher->add();
		return ret;
	}
	return -1;
}

int EventLoop::remove(Channel* channel)
{
	int fd = channel->getSocket();
	if (m_channelMap.find(fd) == m_channelMap.end())
	{
		return -1;
	}
	m_dispatcher->setChannel(channel);
	int ret = m_dispatcher->remove();
	//destroyChannel(evLoop, channel);
	return ret;
}

int EventLoop::modify(Channel* channel)
{
	int fd = channel->getSocket();
	if (m_channelMap.find(fd) == m_channelMap.end())
	{
		return -1;
	}
	m_dispatcher->setChannel(channel);
	int ret = m_dispatcher->modify();
	return ret;
}

int EventLoop::destroyChannel(Channel* channel)
{
	auto it = m_channelMap.find(channel->getSocket());
	if(it != m_channelMap.end())
	{
		m_channelMap.erase(it);
		close(channel->getSocket());
		delete channel;
	}
	return 0;

}

int EventLoop::readLocalMessage(void* arg) {
	EventLoop* evLoop = static_cast<EventLoop*>(arg);
	char buf[256];
	read(evLoop->m_socketpairfd[1], buf, sizeof(buf));
	return 0;
}

int EventLoop::readMessage() {
	char buf[256];
	read(m_socketpairfd[1], buf, sizeof(buf));
	return 0;
}

void EventLoop::taskWakeup()
{
	//向socketpair写入数据，唤醒事件循环
	const char* msg = "WakeUp";
	write(m_socketpairfd[0], msg, strlen(msg));
}

