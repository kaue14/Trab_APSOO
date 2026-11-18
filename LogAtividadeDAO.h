#ifndef LOGATIVIDADEDAO_H
#define LOGATIVIDADEDAO_H

#include "Database.h"
#include "LogAtividade.h"
#include <string>
#include <vector>
#include <iostream>

// RF011 — Grava e consulta o histórico de ações (log de atividades)
class LogAtividadeDAO {
private:
    sqlite3* db;

    static std::string lerTexto(sqlite3_stmt* stmt, int coluna) {
        const unsigned char* raw = sqlite3_column_text(stmt, coluna);
        return raw ? reinterpret_cast<const char*>(raw) : "";
    }

public:
    LogAtividadeDAO(Database& conexao) {
        this->db = conexao.getDb();
    }

    // [CREATE] Registra uma ação com a data e a hora atuais do computador
    bool registrar(int profissionalId, const std::string& tipo, const std::string& acao, const std::string& detalhe) {
        std::string sql = "INSERT INTO log_atividades (data_hora, profissional_id, tipo, acao, detalhe) "
                          "VALUES (datetime('now', 'localtime'), ?, ?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro LogAtividadeDAO] Falha ao preparar INSERT: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, profissionalId);
        sqlite3_bind_text(stmt, 2, tipo.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, acao.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, detalhe.c_str(), -1, SQLITE_TRANSIENT);

        bool sucesso = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!sucesso) std::cerr << "[Erro LogAtividadeDAO] Falha ao registrar: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return sucesso;
    }

    // [READ] As ações mais recentes primeiro, com o nome de quem fez
    std::vector<LogAtividade> listarRecentes(int limite = 500) {
        std::vector<LogAtividade> lista;
        std::string sql = "SELECT l.id, l.data_hora, l.profissional_id, p.nome, l.tipo, l.acao, l.detalhe "
                          "FROM log_atividades l LEFT JOIN profissionais p ON p.id = l.profissional_id "
                          "ORDER BY l.id DESC LIMIT ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, limite);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                lista.push_back(LogAtividade(sqlite3_column_int(stmt, 0), lerTexto(stmt, 1), sqlite3_column_int(stmt, 2),
                                             lerTexto(stmt, 3), lerTexto(stmt, 4), lerTexto(stmt, 5), lerTexto(stmt, 6)));
            }
        } else {
            std::cerr << "[Erro LogAtividadeDAO] Falha ao listar: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_finalize(stmt);
        return lista;
    }
};

#endif
