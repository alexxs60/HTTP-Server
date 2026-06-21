#include "Dispatcher.h"
#include <sys/select.h>
#include "SelectDispatcher.h"


SelectDispatcher::SelectDispatcher(EventLoop* evLoop) :Dispatcher(evLoop)
{
	FD_ZERO(&m_readfds);
	FD_ZERO(&m_writefds);
	m_name = "Select";
}

SelectDispatcher::~SelectDispatcher()
{
}

int SelectDispatcher::add()
{
	if (m_channel->getSocket() >= m_MAX_SIZE) {
		return -1;
	}
	setFdSet();
	return 0;
}

int SelectDispatcher::remove()
{
	clearFdSet();
	//通过channel释放对应的TcpConnection资源
	m_channel->destroyCallback(const_cast<void*>(m_channel->getArg()));
	return 0;
}

int SelectDispatcher::modify()
{
	if (m_channel->getEvents() & static_cast<int>(FDEvent::READ_EVENT)) {
		FD_SET(m_channel->getSocket(), &m_readfds);
	}
	else {
		FD_CLR(m_channel->getSocket(), &m_readfds);
	}

	if (m_channel->getEvents() & static_cast<int>(FDEvent::WRITE_EVENT)) {
		FD_SET(m_channel->getSocket(), &m_writefds);
	}
	else {
		FD_CLR(m_channel->getSocket(), &m_writefds);
	}
	return 0;
}

int SelectDispatcher::dispatch(int time_out)
{
	struct timeval val;
	val.tv_sec = time_out;
	val.tv_usec = 0;
	fd_set readtmp = m_readfds;
	fd_set writetmp = m_writefds;
	int count = select(m_MAX_SIZE, &readtmp, &writetmp, NULL, &val);
	if (count == -1) {
		perror("select");
		exit(0);
	}
	for (int i = 0; i < m_MAX_SIZE; ++i) {
		if (FD_ISSET(i, &readtmp)) {
			m_evLoop->eventActive(i, static_cast<int>(FDEvent::READ_EVENT));
		}
		if (FD_ISSET(i, &writetmp)) {
			m_evLoop->eventActive(i, static_cast<int>(FDEvent::WRITE_EVENT));
		}
	}
	return 0;

}

void SelectDispatcher::setFdSet()
{
	if (m_channel->getEvents() & static_cast<int>(FDEvent::READ_EVENT)) {
		FD_SET(m_channel->getSocket(), &m_readfds);
	}
	if (m_channel->getEvents() & static_cast<int>(FDEvent::WRITE_EVENT)) {
		FD_SET(m_channel->getSocket(), &m_writefds);
	}
}

void SelectDispatcher::clearFdSet()
{
	FD_CLR(m_channel->getSocket(), &m_readfds);
	FD_CLR(m_channel->getSocket(), &m_writefds);
}



