#include "MysqlConn.h"

MysqlConn::MysqlConn() : m_conn(mysql_init(nullptr))
{
	refreshAliveTime();
}

MysqlConn::~MysqlConn()
{
	if (m_conn != nullptr) mysql_close(m_conn);
}

bool MysqlConn::connect(const string& user, const string& password,
	const string& database, const string& host, unsigned short port)
{
	if (m_conn == nullptr) return false;
	MYSQL* connected = mysql_real_connect(m_conn, host.c_str(), user.c_str(),
		password.c_str(), database.c_str(), port, nullptr, 0);
	if (connected == nullptr || mysql_set_character_set(m_conn, "utf8mb4") != 0)
		return false;
	refreshAliveTime();
	return true;
}

bool MysqlConn::update(const string& sql)
{
	return m_conn != nullptr && mysql_query(m_conn, sql.c_str()) == 0;
}

string MysqlConn::escapeString(const string& value)
{
	if (m_conn == nullptr) return string();
	string escaped(value.size() * 2 + 1, '\0');
	unsigned long length = mysql_real_escape_string(m_conn, &escaped[0], value.data(),
		static_cast<unsigned long>(value.size()));
	escaped.resize(length);
	return escaped;
}

string MysqlConn::error() const
{
	return m_conn == nullptr ? "mysql connection is not initialized" : mysql_error(m_conn);
}

bool MysqlConn::ping()
{
	return m_conn != nullptr && mysql_ping(m_conn) == 0;
}

void MysqlConn::refreshAliveTime()
{
	m_aliveTime = chrono::steady_clock::now();
}

long long MysqlConn::getAliveTime() const
{
	return chrono::duration_cast<chrono::milliseconds>(
		chrono::steady_clock::now() - m_aliveTime).count();
}
