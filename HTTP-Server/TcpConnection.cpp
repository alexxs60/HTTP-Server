#include "TcpConnection.h"
#include "DatabaseLogger.h"
#include <utility>

TcpConnection::TcpConnection(EventLoop* evLoop, int fd, const string& clientIp)
{
	m_evLoop = evLoop;
	m_readBuf = new Buffer(10240);
	m_writeBuf = new Buffer(10240);
	m_request = new HttpRequest;
	m_response = new HttpResponse;
	m_name = "Connection-" + to_string(fd);
	m_clientIp = clientIp;
	m_requestStarted = false;
	m_accessLogged = false;
	m_finalStatusCode = 0;
	m_responseBytes = 0;
	m_channel = new Channel(fd, FDEvent::READ_EVENT, processRead, processWrite, destroy, this);
	evLoop->addTask(m_channel, ElemType::ADD);
	/*Debug("Connecting with client, threadName: %s, threadId:%d, connName: %s",
		evLoop->threadName, evLoop->threadId, conn->name);*/
#ifdef ENABLE_ZERO_COPY
	m_fileFd = -1;
	m_fileOffset = 0;
	m_fileSize = 0;
	m_sendingFile = false;
#endif

}

TcpConnection::~TcpConnection()
{
	submitAccessLog();
	delete m_readBuf;
	m_readBuf = nullptr;

	delete m_writeBuf;
	m_writeBuf = nullptr;

	delete m_request;
	m_request = nullptr;

	delete m_response;
	m_response = nullptr;

	if (m_evLoop != nullptr && m_channel != nullptr)
	{
		m_evLoop->destroyChannel(m_channel);
		m_channel = nullptr;
	}
#ifdef ENABLE_ZERO_COPY
	if (m_fileFd != -1) {
		close(m_fileFd);
		m_fileFd = -1;
	}
#endif
	Debug("Connection disconnected, connName: %s", m_name.data());
}

int TcpConnection::processRead(void* arg)
{
	TcpConnection* conn = static_cast<TcpConnection*>(arg);
	int socket = conn->m_channel->getSocket();

	int count = conn->m_readBuf->socketRead(socket);

	if (count > 0)
	{
		if (!conn->m_requestStarted)
		{
			conn->m_requestStart = chrono::steady_clock::now();
			conn->m_requestStarted = true;
		}
		HttpParseResult result = conn->m_request->parseHttpRequest(
			conn->m_readBuf,
			conn->m_response,
			conn->m_writeBuf,
			socket
		);

		if (result == HttpParseResult::Incomplete)
		{
			return 0;
		}

		if (result == HttpParseResult::Error)
		{
			conn->m_finalStatusCode = 400;
			string errMsg = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
			conn->m_writeBuf->appendString(errMsg);
		}
		else if (result == HttpParseResult::Complete)
		{
			conn->m_finalStatusCode = conn->m_response->getStatusCode();
		}
#ifdef ENABLE_ZERO_COPY
		//零拷贝
		if (result == HttpParseResult::Complete && conn->m_response->isFileBody()) //若为文件，则在这里初始化sendfile需要的参数
		{
			conn->m_fileFd = open(conn->m_response->getFileName().data(), O_RDONLY);
			if (conn->m_fileFd == -1) {
				perror("open");
				conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
				return 0;
			}

			conn->m_fileOffset = 0;
			conn->m_fileSize = conn->m_response->getFileSize();
			conn->m_sendingFile = true;
		}
#endif

#ifndef MSG_SEND_AUTO
		conn->m_channel->writeEventEnable(true);
		conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
#endif
	}
	else if (count == -2) //EAGAIN 稍后再试
	{
		return 0;
	}
	else //count==-1 确实出现了错误 或 count==0 对端已断开
	{
		conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
		return 0;
	}

#ifdef MSG_SEND_AUTO
	if (conn->m_writeBuf->readableBytes() == 0)
	{
		conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
	}
	else
	{
		conn->m_channel->writeEventEnable(true);
		conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
	}
#endif

	return 0;
}

int TcpConnection::processWrite(void* arg)
{
	TcpConnection* conn = static_cast<TcpConnection*>(arg);

#ifndef ENABLE_ZERO_COPY
	int count = conn->m_writeBuf->socketWrite(conn->m_channel->getSocket());
	if (count > 0) conn->m_responseBytes += static_cast<uint64_t>(count);

	if (count > 0)
	{
		if (conn->m_writeBuf->readableBytes() == 0)
		{
			conn->submitAccessLog();
			conn->m_channel->writeEventEnable(false);
			conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
			return 0;
		}
		else
		{
			conn->m_channel->writeEventEnable(true);
			conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
		}
	}
	else if (count == 0)
	{
		conn->m_channel->writeEventEnable(true);
		conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
	}
	else
	{
		conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
		return 0;
	}

	return 0;
#else
	int socket = conn->m_channel->getSocket();

	if (conn->m_writeBuf->readableBytes() > 0) {
		int count = conn->m_writeBuf->socketWrite(socket); //除了文件以外的类型都会在这里被发送
		if (count > 0) conn->m_responseBytes += static_cast<uint64_t>(count);

		if (count < 0) {
			conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
			return 0;
		}

		if (conn->m_writeBuf->readableBytes() > 0) {
			conn->m_channel->writeEventEnable(true);
			conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
			return 0;
		}
	}

	if (conn->m_sendingFile) {
		while (conn->m_fileOffset < conn->m_fileSize) {
			ssize_t ret = sendfile(
				socket,
				conn->m_fileFd,
				&conn->m_fileOffset,
				conn->m_fileSize - conn->m_fileOffset
			);

			if (ret > 0) {
				conn->m_responseBytes += static_cast<uint64_t>(ret);
				continue;
			}

			if (ret == 0) {
				break;
			}

			if (ret == -1 && errno == EINTR) {
				continue;
			}

			if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				conn->m_channel->writeEventEnable(true);
				conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
				return 0;
			}

			// 客户端主动断开，比如浏览器暂停音频、拖动进度条、关闭页面
			if (ret == -1 && (errno == EPIPE || errno == ECONNRESET)) {
				conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
				return 0;
			}

			perror("sendfile");
			conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
			return 0;
		}

		close(conn->m_fileFd);
		conn->m_fileFd = -1;
		conn->m_sendingFile = false;
	}

	conn->submitAccessLog();
	conn->m_channel->writeEventEnable(false);
	conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
	return 0;
#endif
}

void TcpConnection::submitAccessLog()
{
	if (m_accessLogged || !m_requestStarted) return;

	const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
		chrono::steady_clock::now() - m_requestStart);

	AccessRecord record;
	record.clientIp = m_clientIp;
	record.method = m_request->getMethod().empty() ? "-" : m_request->getMethod();
	record.url = m_request->getUrl().empty() ? "-" : m_request->getUrl();
	record.httpVersion = m_request->getVersion().empty() ? "-" : m_request->getVersion();
	record.userAgent = m_request->getHeaderIgnoreCase("User-Agent");
	record.statusCode = m_finalStatusCode;
	record.responseBytes = m_responseBytes;
	record.durationMs = static_cast<uint64_t>(elapsed.count());
	DatabaseLogger::getInstance().submit(move(record));
	m_accessLogged = true;
}

int TcpConnection::destroy(void* arg)
{	
	TcpConnection* conn = static_cast<TcpConnection*>(arg);
	if (conn != nullptr)
	{
		delete conn;
	}
	return 0;
}

