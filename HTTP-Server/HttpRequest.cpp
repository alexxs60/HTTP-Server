#include "HttpRequest.h"

HttpRequest::HttpRequest()
{
	reset();
}

HttpRequest::~HttpRequest()
{
}

void HttpRequest::reset()
{
	m_curState = HttpRequestState::ParseReqLine;
	m_method = string();
	m_url = string();
	m_version = string();
	m_reqHeaders.clear();

}

void HttpRequest::addHeader(const string key, const string value)
{
	if(key.empty() || value.empty())
	{
		return;
	}
	m_reqHeaders.insert(make_pair(key, value));
}

string HttpRequest::getHeader(const string key)
{
	auto it = m_reqHeaders.find(key);
	if (it == m_reqHeaders.end())
	{
		return string();
	}
	return it->second;
}

string HttpRequest::getHeaderIgnoreCase(const string& key) const
{
	for (const auto& header : m_reqHeaders)
	{
		if (strcasecmp(header.first.c_str(), key.c_str()) == 0) return header.second;
	}
	return string();
}

HttpParseResult HttpRequest::parseRequestLine(Buffer* readBuf)
{
	char* end = readBuf->findCRLF();
	if (end == nullptr) {
		return HttpParseResult::Incomplete;
	}

	char* start = readBuf->getData() + readBuf->getReadPos();
	int lineLength = end - start;

	if (lineLength <= 0)
	{
		return HttpParseResult::Error;
	}

	auto methodFunc = bind(&HttpRequest::setMethod, this, placeholders::_1);
	start = splitRequestLine(start, end, " ", methodFunc);
	if (start == nullptr) {
		return HttpParseResult::Error;
	}

	auto urlFunc = bind(&HttpRequest::setUrl, this, placeholders::_1);
	start = splitRequestLine(start, end, " ", urlFunc);
	if (start == nullptr) {
		return HttpParseResult::Error;
	}

	auto versionFunc = bind(&HttpRequest::setVersion, this, placeholders::_1);
	if (splitRequestLine(start, end, nullptr, versionFunc) == nullptr) {
		return HttpParseResult::Error;
	}

	//可以挡住明显非法的请求行
	if (m_method.empty() || m_url.empty() || m_version.empty()) {
		return HttpParseResult::Error;
	}

	if (strcasecmp(m_version.data(), "HTTP/1.1") != 0 &&
		strcasecmp(m_version.data(), "HTTP/1.0") != 0) {
		return HttpParseResult::Error;
	}

	readBuf->readPosIncrease(lineLength);
	readBuf->readPosIncrease(2);

	setState(HttpRequestState::ParseReqHeaders);
	return HttpParseResult::Complete;
}

HttpParseResult HttpRequest::parseRequestHeaders(Buffer* readBuf)
{
	char* end = readBuf->findCRLF();
	if (end == nullptr) {
		return HttpParseResult::Incomplete;
	}

	char* start = readBuf->getData() + readBuf->getReadPos();
	int lineLength = end - start;

	if (lineLength == 0) {
		readBuf->readPosIncrease(2);
		setState(HttpRequestState::ParseReqDone);
		return HttpParseResult::Complete;
	}

	char* middle = static_cast<char*>(memmem(start, lineLength, ": ", 2));
	if (middle == nullptr)
	{
		return HttpParseResult::Error;
	}

	int keyLen = middle - start;
	int valLen = end - middle - 2;

	if (keyLen <= 0 || valLen <= 0)
	{
		return HttpParseResult::Error;
	}

	string key(start, keyLen);
	string value(middle + 2, valLen);
	addHeader(key, value);

	readBuf->readPosIncrease(lineLength);
	readBuf->readPosIncrease(2);

	return HttpParseResult::Complete;
}

HttpParseResult HttpRequest::parseHttpRequest(Buffer* readBuf, HttpResponse* res, Buffer* sendBuf, int socket)
{
	HttpParseResult result = HttpParseResult::Complete;

	while (m_curState != HttpRequestState::ParseReqDone)
	{
		switch (m_curState)
		{
		case HttpRequestState::ParseReqLine:
			result = parseRequestLine(readBuf);
			break;
		case HttpRequestState::ParseReqHeaders:
			result = parseRequestHeaders(readBuf);
			break;
		case HttpRequestState::ParseReqBody:
			m_curState = HttpRequestState::ParseReqDone;
			result = HttpParseResult::Complete;
			break;
		default:
			return HttpParseResult::Error;
		}

		if (result != HttpParseResult::Complete)
		{
			return result;
		}

		if (m_curState == HttpRequestState::ParseReqDone)
		{
			bool ok = processHttpRequest(res);
			res->buildResponse(sendBuf, socket);
			m_curState = HttpRequestState::ParseReqLine;
			return ok ? HttpParseResult::Complete : HttpParseResult::Error;
		}
	}

	return HttpParseResult::Complete;
}

bool HttpRequest::processHttpRequest(HttpResponse* res)
{
	if (strcasecmp(m_method.data(), "get") != 0) {
		res->setStatusCode(HttpStatusCode::BadRequest);
#ifdef ENABLE_ZERO_COPY
		res->addHeader("Content-Type", getFileType(".html"));
		res->setBufferBody("400.html");
#else
		res->setFileName("400.html");
		res->addHeader("Content-Type", getFileType(".html"));
#endif
		res->sendDataFunc = sendFile;
		return true;
	}
	m_url = decodeMsg(m_url);

	if (m_url.empty() || m_url[0] != '/') {
		res->setStatusCode(HttpStatusCode::BadRequest);
#ifdef ENABLE_ZERO_COPY
		res->addHeader("Content-Type", getFileType(".html"));
		res->setBufferBody("400.html");
#else
		res->setFileName("400.html");
		res->addHeader("Content-Type", getFileType(".html"));
#endif
		res->sendDataFunc = sendFile;
		return true;
	}
	//防止访问上级目录
	if (m_url.find("..") != string::npos) {
		res->setStatusCode(HttpStatusCode::BadRequest);
#ifdef ENABLE_ZERO_COPY
		res->addHeader("Content-Type", getFileType(".html"));
		res->setBufferBody("400.html");
#else
		res->setFileName("400.html");
		res->addHeader("Content-Type", getFileType(".html"));
#endif
		res->sendDataFunc = sendFile;
		return true;
	}

	//处理客户端请求的静态资源（目录或文件）
	const char* file = NULL;
	if (strcmp(m_url.data(), "/") == 0) {
		file = "./";
	}
	else {
		file = m_url.data() + 1;
	}

	//获取文件属性
	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1) {
		//404
		//sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
		//sendFile("404.html", cfd);
#ifdef ENABLE_ZERO_COPY
		res->setStatusCode(HttpStatusCode::NotFound);
		res->addHeader("Content-Type", getFileType(".html"));
		res->setBufferBody("404.html");
#else
		res->setFileName("404.html");
		res->setStatusCode(HttpStatusCode::NotFound);
		res->addHeader("Content-Type", getFileType(".html"));
#endif
		res->sendDataFunc = sendFile;
		return true;
	}
	res->setFileName(file);
	res->setStatusCode(HttpStatusCode::OK);
	//判断文件类型
	if (S_ISDIR(st.st_mode)) {
		//目录
		//sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
		//sendDir(file, cfd);
		//响应头
		res->addHeader("Content-Type", getFileType(".html"));
#ifdef ENABLE_ZERO_COPY
		res->setBufferBody(file);		//目录仍然用缓冲区去发送
#endif
		res->sendDataFunc = sendDir;
		return true;
	}
	else if (S_ISREG(st.st_mode)) {
		//普通文件
		//sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		//sendFile(file, cfd);
		//响应头
		res->addHeader("Content-Type", getFileType(file));
		res->addHeader("Content-Length", to_string(st.st_size));
#ifdef ENABLE_ZERO_COPY
		res->setFileBody(file, st.st_size);
#else
		res->sendDataFunc = sendFile;
#endif		//文件采用sendfile 直接由内核发送。只有文件采用这种方式，其他为缓冲区
		return true;
	}
	else {
		//其他类型文件，暂不处理
	}

	return false;
}

string HttpRequest::decodeMsg(string msg)
{
	string str = string();
	const char* from = msg.data();
	for (; *from != '\0'; ++from) {
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
			str.append(1,hexToDec(from[1]) * 16 + hexToDec(from[2]));
			from += 2;
		}
		else {
			str.append(1, *from);
		}
	}

	return str;
}

int HttpRequest::hexToDec(char ch) {
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return 0;  // 非法字符
}

const string HttpRequest::getFileType(const string name)
{
	const char* dot = strrchr(name.data(), '.');
	if (dot == NULL)
		return "text/plain; charset=utf-8";
	if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0)
		return "text/html; charset=utf-8";
	if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)
		return "image/jpeg";
	if (strcasecmp(dot, ".png") == 0)
		return "image/png";
	if (strcasecmp(dot, ".css") == 0)
		return "text/css";
	if (strcasecmp(dot, ".js") == 0)
		return "application/javascript";
	if (strcasecmp(dot, ".json") == 0)
		return "application/json";
	if (strcasecmp(dot, ".gif") == 0)
		return "image/gif";
	if (strcasecmp(dot, ".svg") == 0)
		return "image/svg+xml";
	if (strcasecmp(dot, ".ico") == 0)
		return "image/x-icon";
	if (strcasecmp(dot, ".au") == 0)
		return "audio/basic";
	if (strcasecmp(dot, ".wav") == 0)
		return "audio/wav";
	if (strcasecmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	if (strcasecmp(dot, ".mov") == 0)
		return "video/quicktime";
	if (strcasecmp(dot, ".mpeg") == 0 || strcasecmp(dot, ".mpg") == 0)
		return "video/mpeg";
	if (strcasecmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	/*if (strcasecmp(dot, ".flac") == 0)
		return "audio/flac";*/
	if (strcasecmp(dot, ".vrml") == 0 || strcasecmp(dot, ".wrl") == 0)
		return "model/vrml";
	if (strcasecmp(dot, ".ogg") == 0)
		return "audio/ogg";


	return "text/plain; charset=utf-8";
}

void HttpRequest::sendDir(const string dirName, Buffer* sendBuf, int socket)
{
	char buf[4096] = { 0 };
	sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName.data());
	struct dirent** namelist;
	int num = scandir(dirName.data(), &namelist, NULL, alphasort);
	if (num < 0) {
		perror("scandir");
		return;
	}
	for (int i = 0; i < num; i++) {
		//取出文件名 namelist 指向的是一个指针数组 struct dirent* tmp[]
		char* name = namelist[i]->d_name;
		struct stat st;
		char subPath[1024] = { 0 };
		sprintf(subPath, "%s/%s", dirName.data(), name);
		stat(subPath, &st);
		if (S_ISDIR(st.st_mode))
		{
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
				name, name, st.st_size);
		}
		else
		{
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				name, name, st.st_size);
		}
		//send(cfd, buf, strlen(buf), 0);
		sendBuf->appendString(buf);
#ifdef MSG_SEND_AUTO
		sendBuf->socketWrite(socket);
#endif
		memset(buf, 0, sizeof(buf));
		free (namelist[i]);
	}
	sprintf(buf, "</table></body></html>");
	//send(cfd, buf, strlen(buf), 0);
	sendBuf->appendString(buf);
#ifdef MSG_SEND_AUTO
	sendBuf->socketWrite(socket);
#endif
	free (namelist);
	return;
}

void HttpRequest::sendFile(const string fileName, Buffer* sendBuf, int socket)
{
	//打开文件
	int fd = open(fileName.data(), O_RDONLY);
	if (fd == -1) {
		perror("open");
		return;
	}
#if 1
	while (1) {
		char buf[65536] = { 0 };
		int len = read(fd, buf, sizeof buf);
		if (len > 0) {
			//send(cfd, buf, len, 0);
			sendBuf->appendString(buf, len);
#ifdef MSG_SEND_AUTO
			sendBuf->socketWrite(socket);
#endif
			//usleep(10);//很重要
		}
		else if (len == 0) {
			break;
		}
		else {
			close(fd);
			perror("read");
			return;
		}
	}
#else	
	off_t offset = 0;
	int size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	while (offset < size) {
		ssize_t ret = sendfile(cfd, fd, &offset, size - offset);
		if (ret == -1) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				usleep(1000);
				continue;
			}

			perror("sendfile");
			close(fd);
			return -1;
		}
		if (ret == 0) {
			break;
		}

	}
#endif
	close(fd);
	return;
}


char* HttpRequest::splitRequestLine(const char* start, const char* end, const char* sub, function<void(string)> callback)
{
	if (start == nullptr || end == nullptr || start >= end) {
		return nullptr;
	}

	char* space = const_cast<char*>(end);
	if (sub != nullptr)
	{
		space = static_cast<char*>(memmem(start, end - start, sub, strlen(sub)));
		if (space == nullptr) {
			return nullptr;
		}
	}

	int length = space - start;
	if (length <= 0) {
		return nullptr;
	}

	callback(string(start, length));
	return sub == nullptr ? space : space + strlen(sub);
}
