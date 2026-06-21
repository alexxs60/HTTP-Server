#pragma once
#include "HttpResponse.h"
#include <string.h>
#include <strings.h>
#include<string>
#include "Buffer.h"
#include <cassert>
#include <ctype.h>
#include<sys/stat.h>
#include<cstdio>
#include <dirent.h> // 添加头文件
#include <sys/socket.h>
#include <unistd.h> // 添加此头文件以修复 close 未定义的问题
#include <sys/sendfile.h> // 在文件顶部添加以下头文件以修复 sendfile 未定义的问题
#include <fcntl.h> // 添加此头文件以修复 open 和 O_RDONLY 未定义的问题
#include "Config.h"
#include<assert.h>
#include<map>
#include<functional>
#include<errno.h>

using namespace std;
 


//当前解析状态
enum class HttpRequestState:char
{
	ParseReqLine,
	ParseReqHeaders,
	ParseReqBody,
	ParseReqDone
};

enum class HttpParseResult : char
{
	Complete,
	Incomplete,
	Error
};
//定义HttpRequest结构体
class HttpRequest
{
public:
	HttpRequest();
	~HttpRequest();
	//重置
	void reset(); 
	//获取处理状态
	inline HttpRequestState getState() { return m_curState; }
	//添加请求头
	void addHeader(const string key, const string value);//const?
	//根据key获取value
	string getHeader(const string key);//const？
	//解析请求行
	HttpParseResult parseRequestLine(Buffer* redBuf);
	//解析请求头
	HttpParseResult parseRequestHeaders(Buffer* readBuf);
	//解析http请求协议
	HttpParseResult parseHttpRequest(Buffer* readBuf, HttpResponse* res, Buffer* sendBuf, int socket);
	//处理http请求协议
	bool processHttpRequest(HttpResponse* res);
	//解码字符串
	string decodeMsg(string from);
	//获取文件类型
	const string getFileType(const string name);
	static void sendDir(const string dirName, Buffer* sendBuf, int socket);
	static void sendFile(const string fileName, Buffer* sendBuf, int socket);
	inline void setMethod(const string& method) { m_method = method; }
	inline void setUrl(const string& url) { m_url = url; }
	inline void setVersion(const string& version) { m_version = version; }
	inline void setState(HttpRequestState state) { m_curState = state; }
	inline const string& getMethod() const { return m_method; }
	inline const string& getUrl() const { return m_url; }
	inline const string& getVersion() const { return m_version; }
	string getHeaderIgnoreCase(const string& key) const;
private:
	char* splitRequestLine(const char* start, const char* end, const char* sub, function<void(string)> callback);
	int hexToDec(char ch);
private:
	string m_method;
	string m_url;
	string m_version;
	map<string, string> m_reqHeaders;
	HttpRequestState m_curState;

};

