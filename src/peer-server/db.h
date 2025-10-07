//
// Created by caiiiycuk on 12.08.25.
//

#ifndef DB_H
#define DB_H

#include "sqlite3.h"
#include "humblenet.h"
#include <string>
#include <unordered_map>

namespace db {
    class DB {
        sqlite3 *sql;
        sqlite3_stmt* insertLiveStmt;
        sqlite3_stmt* removeLiveStmt; 
        sqlite3_stmt* updatePlaytimeStmt;

        std::unordered_map<std::string, uint64_t> aliasAddedAt;
    public:
        DB();
        ~DB();
        void aliasAdded(const char* alias, PeerId peerId);
        void aliasRemoved(const char* alias, PeerId peerId);
    };

    DB* get();
}

#endif //DB_H
