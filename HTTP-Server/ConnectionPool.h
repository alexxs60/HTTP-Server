#pragma once
#include "MysqlConn.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
using namespace std;

class ConnectionPool
{
public:
	ConnectionPool(const string& host, unsigned short port,
		const string& user, const string& password, const string& database,
		int minSize = 4, int maxSize = 8,
		int timeoutMs = 1000, int maxIdleSeconds = 60);
	~ConnectionPool();
	ConnectionPool(const ConnectionPool&) = delete;
	ConnectionPool& operator=(const ConnectionPool&) = delete;
	shared_ptr<MysqlConn> getConnection();

private:
	bool addConnection();
	void produceConnection();
	void recycleConnection();
	string m_host;
	unsigned short m_port;
	string m_user;
	string m_password;
	string m_database;
	int m_minSize;
	int m_maxSize;
	int m_timeoutMs;
	int m_maxIdleSeconds;
	int m_connectionCount = 0;
	queue<MysqlConn*> m_connections;
	mutex m_mutex;
	condition_variable m_cond;
	atomic<bool> m_stopping{ false };
	thread m_producerThread;
	thread m_recyclerThread;
};
