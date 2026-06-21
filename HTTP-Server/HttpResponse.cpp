#include "HttpResponse.h"

HttpResponse::HttpResponse()
{
	m_statusCode = HttpStatusCode::Unknown;
	m_resHeaders.clear();
	m_fileName = string();
	sendDataFunc = nullptr;
#ifdef ENABLE_ZERO_COPY
	m_bodyType = ResponseBodyType::None;
	m_fileSize = 0;
#endif
}

HttpResponse::~HttpResponse()
{
}

void HttpResponse::addHeader(const string key, const string value)
{
	if (key.empty() || value.empty())
	{
		return;
	}
	m_resHeaders.insert(make_pair(key, value));
}

void HttpResponse::buildResponse(Buffer* sendBuf, int socket)
{
	//状态行
	char tmp[1024] = { 0 };
	int code = static_cast<int>(m_statusCode);
	auto it = m_info.find(code);
#ifdef ENABLE_ZERO_COPY
	if (it == m_info.end()) {
		return;
	}
#else
	if (it == m_info.end() || sendDataFunc == nullptr) {
		return;
	}
#endif
	sprintf(tmp, "HTTP/1.1 %d %s\r\n", code, it->second.data());
	 sendBuf->appendString(tmp);
	//响应头
	for (auto it=m_resHeaders.begin();it!= m_resHeaders.end();it++)
	{
		sprintf(tmp, "%s: %s\r\n", it->first.data(), it->second.data());
		sendBuf->appendString(tmp);
	}
	//空行
	sendBuf->appendString("\r\n");
#ifdef MSG_SEND_AUTO
	sendBuf->socketWrite(socket);
#endif
	//响应体
#ifdef ENABLE_ZERO_COPY
	if (m_bodyType == ResponseBodyType::Buffer && sendDataFunc != nullptr) {
		sendDataFunc(m_fileName, sendBuf, socket);
	}
#else
	sendDataFunc(m_fileName, sendBuf, socket);
#endif
}
