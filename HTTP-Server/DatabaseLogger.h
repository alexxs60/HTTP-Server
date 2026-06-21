#pragma once
#include "ConnectionPool.h"
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
using namespace std;

struct DatabaseConfig
{
	string host = "127.0.0.1";
	unsigned short port = 3306;
	string user;
	string password;
	string database;
};

struct AccessRecord
{
	string clientIp;
	string method;
	string url;
	string httpVersion;
	string userAgent;
	int statusCode = 0;
	uint64_t responseBytes = 0;
	uint64_t durationMs = 0;
};

class DatabaseLogger
{
public:
	static DatabaseLogger& getInstance();
	bool start(const DatabaseConfig& config, int databaseThreadCount = 4);
	void stop();
	bool submit(AccessRecord record);

private:
	DatabaseLogger() = default;
	~DatabaseLogger();
	DatabaseLogger(const DatabaseLogger&) = delete;
	DatabaseLogger& operator=(const DatabaseLogger&) = delete;
	void workerLoop();
	bool createTable(MysqlConn& connection);
	bool insertRecord(MysqlConn& connection, const AccessRecord& record);
	static string truncate(const string& value, size_t maxLength);

	unique_ptr<ConnectionPool> m_pool;
	vector<thread> m_workers;
	queue<AccessRecord> m_queue;
	mutex m_mutex;
	condition_variable m_cond;
	bool m_stopping = false;
	bool m_started = false;
	static constexpr size_t MAX_QUEUE_SIZE = 10000;
};
