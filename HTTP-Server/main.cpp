#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "TcpServer.h"
#include "DatabaseLogger.h"
using namespace std;


int main(int argc, char* argv[])
{
	signal(SIGPIPE, SIG_IGN);
#if 1
	if (argc < 3)
	{
		cout << "./a.out port path" << endl;
		return -1;
	}

	int portNum = atoi(argv[1]);
	if (portNum <= 0 || portNum > 65535)
	{
		cout << "invalid port" << endl;
		return -1;
	}

	if (chdir(argv[2]) == -1)
	{
		perror("chdir");
		return -1;
	}
	unsigned short port = static_cast<unsigned short>(portNum);
#else
	unsigned short port = 10000;
	chdir("/home/mylinux/HttpTest");
#endif
	setenv("MYSQL_HOST", "127.0.0.1", 1);
	setenv("MYSQL_PORT", "3306", 1);
	setenv("MYSQL_USER", "root", 1);
	setenv("MYSQL_PASSWORD", "Psr915140003", 1);
	setenv("MYSQL_DATABASE", "testdb", 1);
	const char* mysqlUser = getenv("MYSQL_USER");
	const char* mysqlPassword = getenv("MYSQL_PASSWORD");
	const char* mysqlDatabase = getenv("MYSQL_DATABASE");
	if (mysqlUser != nullptr && mysqlPassword != nullptr && mysqlDatabase != nullptr)
	{
		DatabaseConfig dbConfig;
		const char* mysqlHost = getenv("MYSQL_HOST");
		const char* mysqlPort = getenv("MYSQL_PORT");
		dbConfig.host = mysqlHost == nullptr ? "127.0.0.1" : mysqlHost;
		dbConfig.user = mysqlUser;
		dbConfig.password = mysqlPassword;
		dbConfig.database = mysqlDatabase;
		if (mysqlPort != nullptr)
		{
			int configuredPort = atoi(mysqlPort);
			if (configuredPort > 0 && configuredPort <= 65535)
			{
				dbConfig.port = static_cast<unsigned short>(configuredPort);
			}
		}
		DatabaseLogger::getInstance().start(dbConfig);
	}
	else
	{
		cerr << "database access log disabled: set MYSQL_USER, MYSQL_PASSWORD and MYSQL_DATABASE" << endl;
	}

	TcpServer* server = new TcpServer(port, 4);
	server->starting();
	DatabaseLogger::getInstance().stop();
	delete server;
	return 0;
}
