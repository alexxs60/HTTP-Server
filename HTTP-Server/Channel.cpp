#include "Channel.h"

	Channel::Channel(int fd, FDEvent events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg)
	{
		m_fd = fd;
		m_events = (int)events;
		readCallback = readFunc;
		writeCallback = writeFunc;
		destroyCallback = destroyFunc;
		m_arg = arg;
	}

	void Channel::writeEventEnable(bool flag)
	{
		if (flag) {
			//m_events |= (int)FDEvent::WRITE_EVENT;
			m_events |= static_cast<int>(FDEvent::WRITE_EVENT);
		}
		else {
			m_events &= ~static_cast<int>(FDEvent::WRITE_EVENT);
		}
	}

	bool Channel::isWriteEventEnable()
	{
		return (m_events & static_cast<int>(FDEvent::WRITE_EVENT)) != 0;
	}
