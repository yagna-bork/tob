#ifndef GUARD_SQLITEDB_H
#define GUARD_SQLITEDB_H
#include <sqlite3.h>

class SQLiteDB {
public:
	SQLiteDB()
		: query_buff_sz(1000), conn_success(false)
	{
		query_buff = new char[query_buff_sz];
		init_config();
		if (sqlite3_open(g_config["DB_PATH"].c_str(), &db) == SQLITE_OK) {
			conn_success = true;
		} else {
			std::cerr << "Couldn't open database" << std::endl;
		}
	}

	SQLiteDB(const SQLiteDB &other) = delete;
	SQLiteDB(SQLiteDB &&other) = delete;
	SQLiteDB& operator=(const SQLiteDB &other) = delete;
	SQLiteDB& operator=(SQLiteDB &&other) = delete;

	bool connected() { return conn_success; }
protected:
	sqlite3 *db;
	sqlite3_stmt *stmt;
	// stores most recently made sql query
	char *query_buff; 
	size_t query_buff_sz;
	bool conn_success;
};
#endif
