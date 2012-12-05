/**
 * @file
 * @brief dbm wrapper for SQLite
**/

#ifndef SQLDBM_H
#define SQLDBM_H

#ifdef USE_SQLITE_DBM

#include <sys/types.h>
#include <memory>

#define SQLITE_INT64_TYPE int
#define SQLITE_UINT64_TYPE unsigned int

#include <sqlite3.h>
#include <string>

// A string dbm interface for SQLite. Makes no attempt to store arbitrary
// data, only valid C strings.

class sql_datum
{
public:
    sql_datum();
    sql_datum(const string &s);
    sql_datum(const sql_datum &other);
    virtual ~sql_datum();

    sql_datum &operator = (const sql_datum &other);

    string to_str() const;

public:
    char   *dptr;    // Canonically void*, but we're not a real Berkeley DB.
    size_t dsize;

private:
    bool   need_free;

    void reset();
    void init_from(const sql_datum &other);
};

#define DBM_REPLACE 1

class SQL_DBM
{
public:
    SQL_DBM(const string &db = "", bool readonly = true, bool open = false);
    ~SQL_DBM();

    bool is_open() const;

    int open(const string &db = "");
    void close();

    unique_ptr<string> firstkey();
    unique_ptr<string> nextkey();

    string query(const string &key);
    int insert(const string &key, const string &value);
    int remove(const string &key);

public:
    string error;
    int errc;

private:
    int finalise_query(sqlite3_stmt **query);
    int prepare_query(sqlite3_stmt **query, const char *sql);
    int init_query();
    int init_iterator();
    int init_insert();
    int init_remove();
    int init_schema();
    int ec(int err);

    int try_insert(const string &key, const string &value);
    int do_insert(const string &key, const string &value);
    int do_query(const string &key, string *result);

private:
    sqlite3      *db;
    sqlite3_stmt *s_insert;
    sqlite3_stmt *s_remove;
    sqlite3_stmt *s_query;
    sqlite3_stmt *s_iterator;
    string       dbfile;
    bool readonly;
};

SQL_DBM  *dbm_open(const char *filename, int open_mode, int permissions);
int   dbm_close(SQL_DBM *db);

sql_datum dbm_fetch(SQL_DBM *db, const sql_datum &key);
sql_datum dbm_firstkey(SQL_DBM *db);
sql_datum dbm_nextkey(SQL_DBM *db);
int dbm_store(SQL_DBM *db, const sql_datum &key,
              const sql_datum &value, int overwrite);

typedef sql_datum datum;
typedef SQL_DBM DBM;

#endif

#endif
