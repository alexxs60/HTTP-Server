#include "Dispatcher.h"
#include "PollDispatcher.h"
#include <poll.h>

PollDispatcher::PollDispatcher(EventLoop* evLoop) :Dispatcher(evLoop)
{
	m_maxfd = 0;
	m_fds = new struct pollfd[m_MAX_NODE];
	for (int i = 0; i < m_MAX_NODE; i++) {
		m_fds[i].fd = -1;
		m_fds[i].events = 0;
		m_fds[i].revents = 0;
	}
	m_name = "Poll";
}

PollDispatcher::~PollDispatcher()
{
	delete[] m_fds;
}

int PollDispatcher::add()
{
	int events = 0;
	if (m_channel->getEvents() & static_cast<int>(FDEvent::READ_EVENT)) {
		events |= POLLIN;
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WRITE_EVENT)) {
		events |= POLLOUT;
	}
	int i = 0;
	for (; i < m_MAX_NODE; i++) {
		if (m_fds[i].fd == -1)
		{
			m_fds[i].fd = m_channel->getSocket();
			m_fds[i].events = events;
			if (i > m_maxfd) {
				m_maxfd = i;
			}
			break;
		}
	}
	if (i >= m_MAX_NODE) {
		return -1;
	}
	return 0;
}

int PollDispatcher::remove()
{
	int i = 0;
	for (; i < m_MAX_NODE; i++) {
		if (m_fds[i].fd == m_channel->getSocket())
		{
			m_fds[i].fd = -1;
			m_fds[i].events = 0;
			m_fds[i].revents = 0;
			if (i == m_maxfd) {
				while (m_maxfd >= 0 && m_fds[m_maxfd].fd == -1) {
					m_maxfd--;
				}
			}
			break;
		}
	}
	//通过channel释放对应的TcpConnection资源
	m_channel->destroyCallback(const_cast<void*>( m_channel->getArg()));
	if (i >= m_MAX_NODE) {
		return -1;
	}
	return 0;
}

int PollDispatcher::modify()
{
	int events = 0;
	if (m_channel->getEvents() & static_cast<int>(FDEvent::READ_EVENT)) {
		events |= POLLIN;
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WRITE_EVENT)) {
		events |= POLLOUT;
	}
	int i = 0;
	for (; i < m_MAX_NODE; i++) {
		if (m_fds[i].fd == m_channel->getSocket()) {
			m_fds[i].events = events;
			m_fds[i].revents = 0;
			break;
		}
	}
	if (i >= m_MAX_NODE) {
		return -1;
	}
	return 0;
}

int PollDispatcher::dispatch(int time_out)
{
	int count = poll(m_fds, m_maxfd + 1, time_out * 1000);
	if (count == -1) {
		perror("poll");
		exit(0);
	}
	for (int i = 0; i <= m_maxfd; ++i) {

		if (m_fds[i].fd == -1) {
			continue;
		}
		if (m_fds[i].revents & POLLIN) {
			m_evLoop->eventActive(m_fds[i].fd, static_cast<int>(FDEvent::READ_EVENT));
		}
		if (m_fds[i].revents & POLLOUT) {
			m_evLoop->eventActive(m_fds[i].fd, static_cast<int>(FDEvent::WRITE_EVENT));
		}
	}
	return 0;
}

