#pragma once
#include <string.h>
#include <strings.h>
#include <cstdlib> 
#include<sys/uio.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string>
#include <errno.h>

using namespace std;

class Buffer {
public:
	Buffer(int size);
	~Buffer();

	//扩容缓冲区
	bool expand(int size);
	//得到剩余的可写空间
	inline int writableBytes() { return m_capacity - m_writePos; }
	//得到剩余的可读空间
	inline int readableBytes() { return m_writePos - m_readPos; } 
	//写内存 1、直接写  2、接收套接字数据
	int appendString(const char* data, int size);
	int appendString(const char* data);
	int appendString(const string data);
	int socketRead(int fd);
	//根据\r\n取出一行，返回其在数据块中的位置
	char* findCRLF();
	//发送数据
	int socketWrite(int fd);
	inline char* getData() { return m_data; }
	inline int getReadPos() { return m_readPos; }
	inline int getWritePos() { return m_writePos; }
	inline int readPosIncrease(int count)
	{
		m_readPos += count;
		return m_readPos;
	}
private:
	char* m_data;
	int  m_readPos=0;
	int  m_writePos=0;
	int  m_capacity;
};
