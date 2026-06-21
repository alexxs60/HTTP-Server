#pragma once

#include <mysql.h>
#include <chrono>
#include <string>
using namespace std;


class MysqlConn
{
public:
	MysqlConn();
	~MysqlConn();
	MysqlConn(const MysqlConn&) = delete;
	MysqlConn& operator=(const MysqlConn&) = delete;

	bool connect(const string& user, const string& password,
		const string& database, const string& host, unsigned short port);
	bool update(const string& sql);
	string escapeString(const string& value);
	string error() const;
	bool ping();
	void refreshAliveTime();
	long long getAliveTime() const;

private:
	MYSQL* m_conn;
	chrono::steady_clock::time_point m_aliveTime;
};
