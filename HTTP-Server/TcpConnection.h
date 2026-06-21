#pragma once
#include<stdlib.h>
#include<stdio.h>
#include "EventLoop.h"
#include "Channel.h"
#include "Buffer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Config.h"
#include"Log.h"
#include <chrono>
#include <cstdint>
#include <string>
#ifdef ENABLE_ZERO_COPY
#include <sys/sendfile.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#endif


class TcpConnection
{
public:
	TcpConnection(EventLoop* evLoop, int fd, const string& clientIp);
	~TcpConnection();

	static int processRead(void* arg);
	static int processWrite(void* arg);
	static int destroy(void* arg);
private:
	void submitAccessLog();
	string m_name;
	string m_clientIp;
	chrono::steady_clock::time_point m_requestStart;
	bool m_requestStarted;
	bool m_accessLogged;
	int m_finalStatusCode;
	uint64_t m_responseBytes;
	EventLoop* m_evLoop;
	Channel* m_channel;
	Buffer* m_readBuf;
	Buffer* m_writeBuf;
	HttpRequest* m_request;
	HttpResponse* m_response;
#ifdef ENABLE_ZERO_COPY
	int m_fileFd;
	off_t m_fileOffset;
	off_t m_fileSize;
	bool m_sendingFile;
#endif
};
