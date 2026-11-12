#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"
#include <string>
#include <iostream>

class Database {
private:
    sqlite3* db;

    void inicializarTabelas() {
        const char* sqlSchema = 
            "PRAGMA foreign_keys = ON;"
            "CREATE TABLE IF NOT EXISTS pacientes ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    nome TEXT NOT NULL,"
            "    cpf TEXT UNIQUE NOT NULL,"
            "    data_nascimento TEXT"
            ");"
            "CREATE TABLE IF NOT EXISTS profissionais ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    nome TEXT NOT NULL,"
            "    registro_conselho TEXT UNIQUE NOT NULL,"
            "    nivel_acesso TEXT NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS catalogo_itens ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    codigo TEXT UNIQUE NOT NULL,"
            "    descricao TEXT NOT NULL,"
            "    valor REAL NOT NULL,"
            "    tipo TEXT NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS atendimentos ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    paciente_id INTEGER NOT NULL,"
            "    profissional_id INTEGER NOT NULL,"
            "    data_atendimento TEXT NOT NULL,"
            "    FOREIGN KEY(paciente_id) REFERENCES pacientes(id),"
            "    FOREIGN KEY(profissional_id) REFERENCES profissionais(id)"
            ");"
            "CREATE TABLE IF NOT EXISTS atendimento_itens ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    atendimento_id INTEGER NOT NULL,"
            "    catalogo_id INTEGER NOT NULL,"
            "    quantidade INTEGER NOT NULL,"
            "    FOREIGN KEY(atendimento_id) REFERENCES atendimentos(id),"
            "    FOREIGN KEY(catalogo_id) REFERENCES catalogo_itens(id)"
            ");";

        char* erroMsg = nullptr;
        if (sqlite3_exec(db, sqlSchema, nullptr, nullptr, &erroMsg) != SQLITE_OK) {
            std::cerr << "[Erro Database] Falha ao verificar/criar tabelas: " << erroMsg << std::endl;
            sqlite3_free(erroMsg);
        }
    }

public:
    Database(const std::string& db_path) : db(nullptr) {
        int status = sqlite3_open(db_path.c_str(), &db);
        if (status != SQLITE_OK) {
            std::cerr << "[Erro Database] Falha ao abrir o banco: " << sqlite3_errmsg(db) << std::endl;
        } else {
            std::cout << "[Database] Conectado a '" << db_path << std::endl;
            inicializarTabelas();
        }
    }

    ~Database() {
        if (db) {
            sqlite3_close(db);
            std::cout << "[Database] Conexao encerrada." << std::endl;
        }
    }

    sqlite3* getDb() const {
        return db;
    }
};

#endif
