#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"
#include <string>
#include <iostream>

class Database {
private:
    sqlite3* db;

public:
    Database(const std::string& db_path) {
        int status = sqlite3_open(db_path.c_str(), &db);
        if (status) {
            std::cerr << "Erro ao abrir o banco: " << sqlite3_errmsg(db) << std::endl;
        } else {
            std::cout << "[Database] Conectado ao banco com sucesso!" << std::endl;
        }
    }

    ~Database() {
        sqlite3_close(db);
        std::cout << "[Database] Conexão encerrada." << std::endl;
    }

    sqlite3* getDb() {
        return db;
    }
};

#endif