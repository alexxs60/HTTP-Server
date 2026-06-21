#pragma once
#include<functional>
using namespace std;

//定义函数指针
//typedef int(*handleFunc)(void* arg);
//using handleFunc = int(*)(void* arg);

enum class FDEvent
{
	READ_EVENT = 0x01,
	WRITE_EVENT = 0x02
};

class Channel
{	
public:
	using handleFunc = function<int(void*)>;//使用包装器，统一函数指针和可调用对象的类型
	Channel(int fd, FDEvent events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg);
	//修改fd的写事件
	void writeEventEnable(bool flag);
	//判断是否需要检测写事件
	bool isWriteEventEnable();
	//回调函数
	handleFunc readCallback;
	handleFunc writeCallback;
	handleFunc destroyCallback;
	//取出私有成员
	inline int getSocket()  { return m_fd; }
	inline int getEvents()  { return m_events; }
	inline const void* getArg()  { return m_arg; }
private:
	//文件描述符
	int m_fd;
	//事件类型
	int m_events;
	//回调函数参数
	void* m_arg;
};
