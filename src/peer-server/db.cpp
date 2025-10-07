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
    
    sqlite3_exec(sql,
        "CREATE TABLE IF NOT EXISTS live ("
            "alias TEXT PRIMARY KEY, "
            "peerid TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS live_alias_prefix ON live(alias);"
        
        "CREATE TABLE IF NOT EXISTS playtime ("
            "prefix TEXT PRIMARY KEY, "
            "total INTEGER"
        ");"
        "CREATE INDEX IF NOT EXISTS playtime_prefix ON playtime(prefix);",
        nullptr, nullptr, nullptr);
    sqlite3_exec(sql, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    if (sqlite3_prepare_v2(sql, "INSERT OR REPLACE INTO live (alias, peerid) VALUES (?, ?);", -1, &insertLiveStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare insert live statement: " << sqlite3_errmsg(sql) << std::endl;
        abort();
    }
    if (sqlite3_prepare_v2(sql, "DELETE FROM live WHERE alias = ?;", -1, &removeLiveStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare remove live statement: " << sqlite3_errmsg(sql) << std::endl;
        abort();
    }
    if (sqlite3_prepare_v2(sql, "INSERT INTO playtime (prefix, total) VALUES (?, ?) ON CONFLICT(prefix) DO UPDATE SET total = total + ?;", -1, &updatePlaytimeStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare insert playtime statement: " << sqlite3_errmsg(sql) << std::endl;
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

    std::string aliasKey = std::string(alias) + "-" + std::to_string(peerId);
    aliasAddedAt[aliasKey] = getNowMs();
}

void db::DB::aliasRemoved(const char* alias, PeerId peerId) {
    sqlite3_bind_text(removeLiveStmt, 1, alias, -1, SQLITE_STATIC);
    if (sqlite3_step(removeLiveStmt) != SQLITE_DONE) {
        std::cerr << "Failed to execute remove: " << sqlite3_errmsg(sql) << std::endl;
    }
    sqlite3_reset(removeLiveStmt);

    std::string aliasKey = std::string(alias) + "-" + std::to_string(peerId);
    auto it = aliasAddedAt.find(aliasKey);
    if (it != aliasAddedAt.end()) {
        std::string prefix = alias;
        auto firstDot = prefix.find('.');
        if (firstDot != std::string::npos) {
            auto secondDot = prefix.find('.', firstDot + 1);
            if (secondDot != std::string::npos) {
                prefix = prefix.substr(0, secondDot);
                auto now = getNowMs();
                if (now > it->second) {
                    auto addTime = getNowMs() - it->second;
                    sqlite3_bind_text(updatePlaytimeStmt, 1, prefix.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_int64(updatePlaytimeStmt, 2, addTime);
                    sqlite3_bind_int64(updatePlaytimeStmt, 3, addTime);
                    if (sqlite3_step(updatePlaytimeStmt) != SQLITE_DONE) {
                        std::cerr << "Failed to execute update: " << sqlite3_errmsg(sql) << std::endl;
                    }
                    sqlite3_reset(updatePlaytimeStmt);
                }
            }
        }
        aliasAddedAt.erase(it);
    }
}

db::DB::~DB() {
    sqlite3_finalize(insertLiveStmt);
    sqlite3_finalize(removeLiveStmt);
    sqlite3_finalize(updatePlaytimeStmt);
    sqlite3_close(sql);
    sql = nullptr;
}

db::DB* db::get() {
    static DB instance;
    return &instance;
}