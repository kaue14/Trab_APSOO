#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"
#include <string>
#include <iostream>

class Database {
private:
    sqlite3* db;

    void executar(const std::string& sql, const std::string& descricao) {
        char* erroMsg = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &erroMsg) != SQLITE_OK) {
            std::cerr << "[Erro Database] Falha ao " << descricao << ": " << erroMsg << std::endl;
            sqlite3_free(erroMsg);
        }
    }

    bool colunaExiste(const std::string& tabela, const std::string& coluna) {
        std::string sql = "PRAGMA table_info(" + tabela + ");";
        sqlite3_stmt* stmt;
        bool existe = false;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* nomeRaw = sqlite3_column_text(stmt, 1);
                if (nomeRaw && coluna == reinterpret_cast<const char*>(nomeRaw)) existe = true;
            }
        }
        sqlite3_finalize(stmt);
        return existe;
    }

    // Bancos criados antes destas colunas existirem (ex.: pelo script_bd.py) ganham as colunas novas
    void adicionarColunaSeFaltar(const std::string& tabela, const std::string& coluna, const std::string& tipo) {
        if (!colunaExiste(tabela, coluna)) {
            executar("ALTER TABLE " + tabela + " ADD COLUMN " + coluna + " " + tipo + ";",
                     "adicionar a coluna " + tabela + "." + coluna);
        }
    }

    void inicializarTabelas() {
        const char* sqlSchema =
            "PRAGMA foreign_keys = ON;"
            "CREATE TABLE IF NOT EXISTS pacientes ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    nome TEXT NOT NULL,"
            "    cpf TEXT UNIQUE,"              // RF006: o paciente tem CPF ou carteira
            "    data_nascimento TEXT,"
            "    carteira TEXT,"
            "    rua TEXT,"
            "    complemento TEXT,"
            "    bairro TEXT,"
            "    cidade TEXT,"
            "    uf TEXT"
            ");"
            "CREATE TABLE IF NOT EXISTS profissionais ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    nome TEXT NOT NULL,"
            "    registro_conselho TEXT UNIQUE NOT NULL,"
            "    nivel_acesso TEXT NOT NULL,"
            "    login TEXT,"
            "    cpf TEXT,"
            "    senha_hash TEXT"               // NF004: nunca a senha em texto puro
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
            "    registrado_por INTEGER,"       // RF012: Técnico só edita o que ele registrou
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
        executar(sqlSchema, "verificar/criar tabelas");

        adicionarColunaSeFaltar("pacientes", "carteira", "TEXT");
        adicionarColunaSeFaltar("pacientes", "rua", "TEXT");
        adicionarColunaSeFaltar("pacientes", "complemento", "TEXT");
        adicionarColunaSeFaltar("pacientes", "bairro", "TEXT");
        adicionarColunaSeFaltar("pacientes", "cidade", "TEXT");
        adicionarColunaSeFaltar("pacientes", "uf", "TEXT");
        adicionarColunaSeFaltar("profissionais", "login", "TEXT");
        adicionarColunaSeFaltar("profissionais", "cpf", "TEXT");
        adicionarColunaSeFaltar("profissionais", "senha_hash", "TEXT");
        adicionarColunaSeFaltar("atendimentos", "registrado_por", "INTEGER");

        const char* sqlAjustes =
            // O SQLite não aceita UNIQUE em ALTER TABLE, então a unicidade vem por índice
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_pacientes_carteira ON pacientes(carteira);"
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_profissionais_login ON profissionais(login);"
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_profissionais_cpf ON profissionais(cpf);"
            // Atendimentos antigos: considera que quem registrou foi o profissional executante
            "UPDATE atendimentos SET registrado_por = profissional_id WHERE registrado_por IS NULL;"
            // Separa a tabela de valores em procedimentos e materiais (códigos 0071... são materiais)
            "UPDATE catalogo_itens SET tipo = CASE WHEN codigo LIKE '0071%' THEN 'Material' ELSE 'Procedimento' END "
            "WHERE tipo = 'Geral';";
        executar(sqlAjustes, "ajustar índices e dados");
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
