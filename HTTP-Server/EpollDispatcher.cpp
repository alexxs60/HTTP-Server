#include "Dispatcher.h"
#include <sys/epoll.h>
#include <cerrno>
#include "EpollDispatcher.h"

EpollDispatcher::EpollDispatcher(EventLoop* evLoop) :Dispatcher(evLoop)//委托构造？继承构造？
{
	m_epfd = epoll_create(10);
	if (m_epfd == -1) {
		perror("epoll_create");
		exit(0);
	}
	m_events = new struct epoll_event[m_MAX_EVENTS];
	m_name = "Epoll";
}

EpollDispatcher::~EpollDispatcher()
{
	close(m_epfd);
	delete[] m_events;
}

int EpollDispatcher::add()
{
	int ret = epollCtl(EPOLL_CTL_ADD);
	if (ret == -1) {
		perror("epoll_ctl add"); 
		exit(0);
	}
	return ret;
}

int EpollDispatcher::remove()
{
	int ret = epollCtl(EPOLL_CTL_DEL);
	if (ret == -1) {
		perror("epoll_ctl delete");
		exit(0);
	}
	//通过channel释放对应的TcpConnection资源
	m_channel->destroyCallback(const_cast<void*>(m_channel->getArg()));
	return ret;
}

int EpollDispatcher::modify()
{
	int ret = epollCtl(EPOLL_CTL_MOD);
	if (ret == -1) {
		perror("epoll_ctl modify");
		exit(0);
	}
	return ret;
}

int EpollDispatcher::dispatch(int time_out)
{
	int nfds = epoll_wait(m_epfd, m_events, m_MAX_EVENTS, time_out * 1000);
	if (nfds == -1) {
		if (errno == EINTR) {
			// 被信号中断，不是致命错误，继续下一轮事件循环
			return 0;
		}
		perror("epoll_wait");
		return -1;
	}
	if (nfds == 0) {
		// 超时，没有事件发生
		return 0;
	}

	for (int i = 0; i < nfds; ++i) {
		int fd = m_events[i].data.fd;
		int events = m_events[i].events;
		if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
			m_evLoop->eventActive(fd, static_cast<int>(FDEvent::READ_EVENT));
			continue;
		}
		if (events & EPOLLIN) {
			m_evLoop->eventActive(fd, static_cast<int>(FDEvent::READ_EVENT));
		}
		if (events & EPOLLOUT) {
			m_evLoop->eventActive(fd, static_cast<int>(FDEvent::WRITE_EVENT));
		}
	}
	return 0;

}

int EpollDispatcher::epollCtl(int op)
{
	struct epoll_event ev;
	ev.data.fd = m_channel->getSocket();
	int events = 0;
	if (m_channel->getEvents() & static_cast<int>(FDEvent::READ_EVENT)) {
		events |= EPOLLIN;
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WRITE_EVENT)) {
		events |= EPOLLOUT;
	}
	ev.events = events;
	int ret = epoll_ctl(m_epfd, op, m_channel->getSocket(), &ev);
	return ret;
}
