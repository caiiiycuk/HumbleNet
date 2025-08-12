//
// Created by caiiiycuk on 12.08.25.
//
#include "./db.h"

#include <iostream>
#include <chrono>
#include <string.h>

int64_t getNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

db::DB::DB(): sql(nullptr) {
    if (sqlite3_open_v2("net.sqlite3", &sql, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(sql) << std::endl;
        abort();
    }
    
    sqlite3_exec(sql, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(sql, 
        "CREATE TABLE IF NOT EXISTS live ("
            "alias TEXT PRIMARY KEY, "
            "peerid TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS live_alias_prefix ON live(alias);"
        
        "CREATE TABLE IF NOT EXISTS history ("
            "alias TEXT PRIMARY KEY, "
            "\"startedAt\" INTEGER, "
            "\"endedAt\" INTEGER"
        ");"
        "CREATE INDEX IF NOT EXISTS history_alias_prefix ON history(alias);",
        nullptr, nullptr, nullptr);

    if (sqlite3_prepare_v2(sql, "INSERT OR REPLACE INTO live (alias, peerid) VALUES (?, ?);", -1, &insertLiveStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare insert live statement: " << sqlite3_errmsg(sql) << std::endl;
        abort();
    }
    if (sqlite3_prepare_v2(sql, "DELETE FROM live WHERE alias = ?;", -1, &removeLiveStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare remove live statement: " << sqlite3_errmsg(sql) << std::endl;
        abort();
    }
    if (sqlite3_prepare_v2(sql, "INSERT OR REPLACE INTO history (alias, \"startedAt\", \"endedAt\") VALUES (?, ?, ?);", -1, &insertHistoryStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare insert history statement: " << sqlite3_errmsg(sql) << std::endl;
        abort();
    }
    if (sqlite3_prepare_v2(sql, "UPDATE history SET endedAt = ? WHERE alias = ?;", -1, &updateHistoryStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare update history statement: " << sqlite3_errmsg(sql) << std::endl;
        abort();
    }
}

void db::DB::aliasAdded(const char* alias, PeerId peerId) {
    sqlite3_bind_text(insertLiveStmt, 1, alias, -1, SQLITE_STATIC);
    sqlite3_bind_int64(insertLiveStmt, 2, peerId);
    if (sqlite3_step(insertLiveStmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute insert: " << sqlite3_errmsg(sql) << std::endl;
    }
    sqlite3_reset(insertLiveStmt);

    sqlite3_bind_text(insertHistoryStmt, 1, alias, -1, SQLITE_STATIC);
    sqlite3_bind_int64(insertHistoryStmt, 2, getNowMs());
    sqlite3_bind_int64(insertHistoryStmt, 3, getNowMs());
    if (sqlite3_step(insertHistoryStmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute insert: " << sqlite3_errmsg(sql) << std::endl;
    }
    sqlite3_reset(insertHistoryStmt);
}

void db::DB::aliasRemoved(const char* alias) {
    sqlite3_bind_text(removeLiveStmt, 1, alias, -1, SQLITE_STATIC);
    if (sqlite3_step(removeLiveStmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute remove: " << sqlite3_errmsg(sql) << std::endl;
    }
    sqlite3_reset(removeLiveStmt);

    sqlite3_bind_int64(updateHistoryStmt, 1, getNowMs());
    sqlite3_bind_text(updateHistoryStmt, 2, alias, -1, SQLITE_STATIC);
    if (sqlite3_step(updateHistoryStmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute update: " << sqlite3_errmsg(sql) << std::endl;
    }
    sqlite3_reset(updateHistoryStmt);
}

db::DB::~DB() {
    sqlite3_finalize(insertLiveStmt);
    sqlite3_finalize(removeLiveStmt);
    sqlite3_finalize(insertHistoryStmt);
    sqlite3_finalize(updateHistoryStmt);
    sqlite3_close(sql);
    sql = nullptr;
}

db::DB* db::get() {
    static DB instance;
    return &instance;
}