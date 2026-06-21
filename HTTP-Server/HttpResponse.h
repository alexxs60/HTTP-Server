#pragma once
#include <string.h>
#include <strings.h>
#include "Buffer.h"
#include<cstdio>
#include "Config.h"
#include<string>
#include<map>
#include<functional>
#ifdef ENABLE_ZERO_COPY
#include <sys/types.h>
#endif
using namespace std;

#ifdef ENABLE_ZERO_COPY
enum class ResponseBodyType {
	None,
	Buffer,
	File
};
#endif
//定义HTTP响应状态码枚举
enum class HttpStatusCode {
	Unknown,
	OK=200,
	MovedPermanently = 301,
	MovedTemporarily = 302,
	BadRequest = 400,
	NotFound = 404,
};

class HttpResponse
{	
public:
	HttpResponse();
	~HttpResponse();
	//初始化HttpResponse
	//添加响应头
	void addHeader(const string key, const string value);
	//组织http响应数据
	void buildResponse(Buffer* sendBuf, int socket);

	 function<void(const string ,Buffer*,int)>sendDataFunc;
	 inline void setFileName(string name)
	 {
		 m_fileName = name;
	 }
     // 替换 setStatusCode 方法，接受 HttpStatusCode 类型而不是 string
     inline void setStatusCode(HttpStatusCode code)
     {
         m_statusCode = code;
     }
	 inline int getStatusCode() const { return static_cast<int>(m_statusCode); }
#ifdef ENABLE_ZERO_COPY
	 inline void setFileBody(const string& name, off_t size)
	 {
		 m_fileName = name;
		 m_fileSize = size;
		 m_bodyType = ResponseBodyType::File;
	 }

	 inline void setBufferBody(const string& name)
	 {
		 m_fileName = name;
		 m_bodyType = ResponseBodyType::Buffer;
	 }

	 inline bool isFileBody() const
	 {
		 return m_bodyType == ResponseBodyType::File;
	 }

	 inline const string& getFileName() const
	 {
		 return m_fileName;
	 }

	 inline off_t getFileSize() const
	 {
		 return m_fileSize;
	 }
#endif
private:
	//状态行：状态码，状态描述
	HttpStatusCode m_statusCode;
	string m_fileName;
	//响应头--键值对
	map<string,string> m_resHeaders;
	//定义状态码和描述的对应关系
	const map<int, string> m_info = {
		{200,"OK"},
		{301,"MovedPermanently"},
		{302,"MovedTemporarily"},
		{400,"BadRequest"},
		{404,"NotFound"}
	};
#ifdef ENABLE_ZERO_COPY
	ResponseBodyType m_bodyType;
	off_t m_fileSize;
#endif
	
};

