#include "DatabaseLogger.h"
#include <iostream>
#include <utility>

DatabaseLogger& DatabaseLogger::getInstance()
{
	static DatabaseLogger logger;
	return logger;
}

DatabaseLogger::~DatabaseLogger() { stop(); }

bool DatabaseLogger::start(const DatabaseConfig& config, int databaseThreadCount)
{
	if (databaseThreadCount <= 0) return false;
	{
		lock_guard<mutex> lock(m_mutex);
		if (m_started) return true;
		m_stopping = false;
	}

	unique_ptr<ConnectionPool> pool(new ConnectionPool(
		config.host, config.port, config.user, config.password, config.database,
		databaseThreadCount, databaseThreadCount * 2, 3000, 60));
	auto connection = pool->getConnection();
	if (connection == nullptr || !createTable(*connection))
	{
		if (connection != nullptr)
			cerr << "create access log table failed: " << connection->error() << endl;
		return false;
	}
	connection.reset();
	m_pool = move(pool);
	{
		lock_guard<mutex> lock(m_mutex);
		m_started = true;
	}
	for (int i = 0; i < databaseThreadCount; ++i)
		m_workers.emplace_back(&DatabaseLogger::workerLoop, this);
	cout << "database logger started with " << databaseThreadCount << " workers" << endl;
	return true;
}

void DatabaseLogger::stop()
{
	{
		lock_guard<mutex> lock(m_mutex);
		if (!m_started) return;
		m_stopping = true;
	}
	m_cond.notify_all();
	for (thread& worker : m_workers)
		if (worker.joinable()) worker.join();
	m_workers.clear();
	m_pool.reset();
	{
		lock_guard<mutex> lock(m_mutex);
		m_started = false;
	}
}

bool DatabaseLogger::submit(AccessRecord record)
{
	{
		lock_guard<mutex> lock(m_mutex);
		if (!m_started || m_stopping || m_queue.size() >= MAX_QUEUE_SIZE) return false;
		m_queue.push(move(record));
	}
	m_cond.notify_one();
	return true;
}

void DatabaseLogger::workerLoop()
{
	while (true)
	{
		AccessRecord record;
		{
			unique_lock<mutex> lock(m_mutex);
			m_cond.wait(lock, [this]() { return m_stopping || !m_queue.empty(); });
			if (m_stopping && m_queue.empty()) break;
			record = move(m_queue.front());
			m_queue.pop();
		}
		auto connection = m_pool->getConnection();
		if (connection == nullptr)
		{
			cerr << "get mysql connection failed" << endl;
			continue;
		}
		if (!insertRecord(*connection, record))
			cerr << "insert access log failed: " << connection->error() << endl;
	}
}

bool DatabaseLogger::createTable(MysqlConn& connection)
{
	static const char* sql =
		"CREATE TABLE IF NOT EXISTS http_access_log ("
		"id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
		"client_ip VARCHAR(45) NOT NULL,method VARCHAR(16) NOT NULL,"
		"request_path VARCHAR(2048) NOT NULL,http_version VARCHAR(16) NOT NULL,"
		"status_code SMALLINT UNSIGNED NOT NULL,user_agent VARCHAR(512) NULL,"
		"response_bytes BIGINT UNSIGNED NOT NULL DEFAULT 0,"
		"duration_ms BIGINT UNSIGNED NOT NULL DEFAULT 0,"
		"visited_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),"
		"INDEX idx_visited_at (visited_at),INDEX idx_client_ip (client_ip),"
		"INDEX idx_status_code (status_code)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
	return connection.update(sql);
}

bool DatabaseLogger::insertRecord(MysqlConn& connection, const AccessRecord& record)
{
	const string ip = connection.escapeString(truncate(record.clientIp, 45));
	const string method = connection.escapeString(truncate(record.method, 16));
	const string url = connection.escapeString(truncate(record.url, 2048));
	const string version = connection.escapeString(truncate(record.httpVersion, 16));
	const string userAgent = connection.escapeString(truncate(record.userAgent, 512));
	const string sql =
		"INSERT INTO http_access_log(client_ip,method,request_path,http_version,"
		"status_code,user_agent,response_bytes,duration_ms) VALUES('" + ip + "','" +
		method + "','" + url + "','" + version + "'," + to_string(record.statusCode) +
		",'" + userAgent + "'," + to_string(record.responseBytes) + "," +
		to_string(record.durationMs) + ")";
	return connection.update(sql);
}

string DatabaseLogger::truncate(const string& value, size_t maxLength)
{
	return value.size() <= maxLength ? value : value.substr(0, maxLength);
}
