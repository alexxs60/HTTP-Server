#include "TcpServer.h"

TcpServer::TcpServer(unsigned short port, int threadNum)
{
	m_port = port;
	m_mainLoop = new EventLoop;
	m_threadNum = threadNum;
	m_threadPool = new ThreadPool(m_mainLoop, threadNum);
	setListener();

}

TcpServer::~TcpServer()
{
	if (m_mainLoop != nullptr)
	{
		delete m_mainLoop;
		m_mainLoop = nullptr;
	}
	if (m_threadPool != nullptr)
	{
		delete m_threadPool;
		m_threadPool = nullptr;
	}
}

void TcpServer::setListener()
{
	// 创建监听套接字
	m_lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_lfd == -1) {
		perror("socket");
		return;
	}
	int flags = fcntl(m_lfd, F_GETFL, 0);
	fcntl(m_lfd, F_SETFL, flags | O_NONBLOCK);

	//设置端口复用
	int opt = 1;
	int ret = setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1) {
		perror("setsockopt");
		return;
	}

	//绑定
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_port);
	addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(m_lfd, (struct sockaddr*)&addr, sizeof addr);
	if (ret == -1) {
		perror("bind");
		return;
	}

	//监听
	ret = listen(m_lfd, 128);
	if (ret == -1) {
		perror("listen");
		return;
	}

}

void TcpServer::starting()
{
	Debug("TCP Server is starting...");
	//启动线程池
	m_threadPool->running();
	//添加检测的任务
	//初始化一个Channel实例
#if 1
	Channel* channel = new Channel(m_lfd, FDEvent::READ_EVENT, acceptConnection, nullptr, nullptr, this); //通过静态函数来调用成员函数
#else
	auto obj = bind(&TcpServer::accept_Connection, this);//把非静态成员函数和对象绑定成可调用对象
	Channel* channel = new Channel(m_lfd, FDEvent::READ_EVENT, obj, nullptr, nullptr, nullptr);
#endif
	m_mainLoop->addTask(channel, ElemType::ADD);
	//启动反应堆模型
	m_mainLoop->run();
}

int TcpServer::acceptConnection(void* arg)
{
	TcpServer* server = static_cast<TcpServer*>(arg);
	while (true) {
		struct sockaddr_in clientAddr = {};
		socklen_t clientLen = sizeof(clientAddr);
		int cfd = accept4(server->m_lfd, reinterpret_cast<struct sockaddr*>(&clientAddr),
			&clientLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (cfd == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			}
			if (errno == EINTR) {
				continue;
			}
			perror("accept4");
			return -1;
		}

		char clientIp[INET_ADDRSTRLEN] = { 0 };
		if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp)) == nullptr)
			strcpy(clientIp, "unknown");
		EventLoop* evLoop = server->m_threadPool->takeWorkEventLoop();
		new TcpConnection(evLoop, cfd, clientIp);
	}
	return 0;
}

int TcpServer::accept_Connection()
{
	//和客户端建立连接
	struct sockaddr_in clientAddr = {};
	socklen_t clientLen = sizeof(clientAddr);
	int cfd = accept(m_lfd, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
	if (cfd == -1) {
		perror("accept");
		return -1;
	}
	//从线程池中取出一个子线程的反应堆实例，处理这个cfd
	EventLoop* evLoop = m_threadPool->takeWorkEventLoop();
	//将cfd放到TcpConnection中处理
	//TcpConnection* conn = new TcpConnection(evLoop, cfd);
	char clientIp[INET_ADDRSTRLEN] = { 0 };
	if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp)) == nullptr)
		strcpy(clientIp, "unknown");
	new TcpConnection(evLoop, cfd, clientIp);
	return 0;
}
