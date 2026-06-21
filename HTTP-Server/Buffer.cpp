#include "Buffer.h"

Buffer::Buffer(int size) :m_capacity(size)
{
	m_data = (char*)malloc(size);
	bzero(m_data, size);
}

Buffer::~Buffer()
{
	if (m_data!=nullptr)
	{
		free(m_data);
	}
}

bool Buffer::expand(int size)
{
	if (writableBytes() >= size) {
		return true;
	}
	else if (writableBytes() + m_readPos >= size) {
		int readable = readableBytes();
		memcpy(m_data, m_data + m_readPos, readable);
		m_writePos = readable;
		m_readPos = 0;
		return true;
	}
	else {
		void* temp = realloc(m_data, m_capacity + size);
		if (temp == nullptr) {
			return false;
		}

		memset((char*)temp + m_capacity, 0, size);
		m_data = static_cast<char*>(temp);
		m_capacity += size;
		return true;
	}
}

int Buffer::appendString(const char* data, int size)
{
	if ( data == nullptr || size <= 0) {
		return -1;
	}

	if (!expand(size)) {
		return -1;
	}
	memcpy(m_data + m_writePos, data, size);
	m_writePos += size;
	return 0;
}

int Buffer::appendString(const char* data)
{
	int size = strlen(data);
	int ret = appendString(data, size);
	return ret;
}

int Buffer::appendString(const string data)
{
	int ret = appendString(data.data());
	return ret;
}

int Buffer::socketRead(int fd)
{
	struct iovec vec[2];
	int writeable = writableBytes();

	vec[0].iov_base = m_data + m_writePos;
	vec[0].iov_len = writeable;

	char* tmpbuf = (char*)malloc(40960);
	vec[1].iov_base = tmpbuf;
	vec[1].iov_len = 40960;

	ssize_t result = 0;
	while (true) {
		result = readv(fd, vec, 2);
		if (result == -1 && errno == EINTR) {
			continue;
		}
		break;
	}

	if (result == -1) {
		free(tmpbuf);
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return -2;
		}
		return -1;
	}

	if (result == 0) {
		free(tmpbuf);
		return 0;
	}

	if (result <= writeable)
	{
		m_writePos += static_cast<int>(result);
	}
	else
	{
		m_writePos = m_capacity;
		appendString(tmpbuf, static_cast<int>(result - writeable));
	}

	free(tmpbuf);
	return static_cast<int>(result);
}

char* Buffer::findCRLF()
{
	void* ptr = memmem(m_data + m_readPos, readableBytes(), "\r\n", 2);
	return static_cast<char*>(ptr);
}

int Buffer::socketWrite(int fd)
{
	int readable = readableBytes();
	if (readable > 0)
	{
		ssize_t count = 0;

		while (true) {
			count = send(fd, m_data + m_readPos, readable, MSG_NOSIGNAL);
			if (count == -1 && errno == EINTR) {
				continue;
			}
			break;
		}

		if (count > 0) {
			m_readPos += count;
			return count;
		}

		if (count == -1) {
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;
			}
			return -1;
		}

		return 0;
	}
	return 0;
}
