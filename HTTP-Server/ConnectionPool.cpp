#include "ConnectionPool.h"
#include <chrono>
#include <iostream>

ConnectionPool::ConnectionPool(const string& host, unsigned short port,
	const string& user, const string& password, const string& database,
	int minSize, int maxSize, int timeoutMs, int maxIdleSeconds)
	: m_host(host), m_port(port), m_user(user), m_password(password),
	m_database(database), m_minSize(minSize), m_maxSize(maxSize),
	m_timeoutMs(timeoutMs), m_maxIdleSeconds(maxIdleSeconds)
{
	if (m_minSize < 1) m_minSize = 1;
	if (m_maxSize < m_minSize) m_maxSize = m_minSize;
	for (int i = 0; i < m_minSize; ++i)
		if (!addConnection()) break;
	m_producerThread = thread(&ConnectionPool::produceConnection, this);
	m_recyclerThread = thread(&ConnectionPool::recycleConnection, this);
}

ConnectionPool::~ConnectionPool()
{
	{
		lock_guard<mutex> lock(m_mutex);
		m_stopping = true;
	}
	m_cond.notify_all();
	if (m_producerThread.joinable()) m_producerThread.join();
	if (m_recyclerThread.joinable()) m_recyclerThread.join();
	lock_guard<mutex> lock(m_mutex);
	while (!m_connections.empty())
	{
		delete m_connections.front();
		m_connections.pop();
	}
	m_connectionCount = 0;
}

bool ConnectionPool::addConnection()
{
	MysqlConn* connection = new MysqlConn();
	if (!connection->connect(m_user, m_password, m_database, m_host, m_port))
	{
		cerr << "create mysql connection failed: " << connection->error() << endl;
		delete connection;
		return false;
	}
	connection->refreshAliveTime();
	m_connections.push(connection);
	++m_connectionCount;
	return true;
}

shared_ptr<MysqlConn> ConnectionPool::getConnection()
{
	unique_lock<mutex> lock(m_mutex);
	const auto deadline = chrono::steady_clock::now() + chrono::milliseconds(m_timeoutMs);
	while (!m_stopping)
	{
		while (!m_connections.empty())
		{
			MysqlConn* connection = m_connections.front();
			m_connections.pop();
			if (!connection->ping())
			{
				delete connection;
				--m_connectionCount;
				m_cond.notify_all();
				continue;
			}
			m_cond.notify_all();
			return shared_ptr<MysqlConn>(connection, [this](MysqlConn* conn) {
				lock_guard<mutex> poolLock(m_mutex);
				if (m_stopping)
				{
					delete conn;
					--m_connectionCount;
				}
				else
				{
					conn->refreshAliveTime();
					m_connections.push(conn);
				}
				m_cond.notify_all();
			});
		}

		if (m_connectionCount < m_maxSize && addConnection()) continue;
		if (!m_cond.wait_until(lock, deadline, [this]() {
			return m_stopping || !m_connections.empty();
		}))
		{
			cerr << "get mysql connection timeout" << endl;
			return nullptr;
		}
	}
	return nullptr;
}

void ConnectionPool::produceConnection()
{
	while (true)
	{
		unique_lock<mutex> lock(m_mutex);
		m_cond.wait(lock, [this]() {
			return m_stopping ||
				(m_connections.size() < static_cast<size_t>(m_minSize) &&
				 m_connectionCount < m_maxSize);
		});
		if (m_stopping) break;
		if (!addConnection())
			m_cond.wait_for(lock, chrono::seconds(1), [this]() { return m_stopping.load(); });
		else
			m_cond.notify_all();
	}
}

void ConnectionPool::recycleConnection()
{
	while (true)
	{
		unique_lock<mutex> lock(m_mutex);
		if (m_cond.wait_for(lock, chrono::seconds(1),
			[this]() { return m_stopping.load(); })) break;
		const long long maxIdleMs = static_cast<long long>(m_maxIdleSeconds) * 1000;
		while (m_connections.size() > static_cast<size_t>(m_minSize))
		{
			MysqlConn* connection = m_connections.front();
			if (connection->getAliveTime() < maxIdleMs) break;
			m_connections.pop();
			delete connection;
			--m_connectionCount;
		}
		m_cond.notify_all();
	}
}
