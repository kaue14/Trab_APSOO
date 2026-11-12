#ifndef PROFISSIONALDAO_H
#define PROFISSIONALDAO_H

#include "Database.h"
#include "Profissional.h"
#include <string>
#include <vector>
#include <iostream>

class ProfissionalDAO {
private:
    sqlite3* db;

    // Método auxiliar interno para converter uma linha do SQLite em objeto Profissional
    Profissional extrairDaLinha(sqlite3_stmt* stmt) {
        int id = sqlite3_column_int(stmt, 0);
        
        const unsigned char* nomeRaw = sqlite3_column_text(stmt, 1);
        std::string nome = nomeRaw ? reinterpret_cast<const char*>(nomeRaw) : "";

        const unsigned char* regRaw = sqlite3_column_text(stmt, 2);
        std::string registro = regRaw ? reinterpret_cast<const char*>(regRaw) : "";

        const unsigned char* nivelRaw = sqlite3_column_text(stmt, 3);
        std::string nivel = nivelRaw ? reinterpret_cast<const char*>(nivelRaw) : "";

        return Profissional(id, nome, registro, nivel);
    }

public:
    ProfissionalDAO(Database& conexao) {
        this->db = conexao.getDb();
    }

    // [CREATE] RF002 - Cadastrar usuário (Técnico, Enfermeiro ou Administrador)
    bool inserir(Profissional& prof) {
        std::string sql = "INSERT INTO profissionais (nome, registro_conselho, nivel_acesso) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro ProfissionalDAO] Falha ao preparar INSERT: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, prof.getNome().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, prof.getRegistroConselho().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, prof.getNivelAcesso().c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "[Erro ProfissionalDAO] Falha ao cadastrar profissional: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        prof.setId((int)sqlite3_last_insert_rowid(db));
        sqlite3_finalize(stmt);
        std::cout << "[DAO] Profissional '" << prof.getNome() << "' (" << prof.getNivelAcesso() 
                  << ") cadastrado com ID " << prof.getId() << "!" << std::endl;
        return true;
    }

    // Autenticar usuário RF001
    Profissional buscarPorRegistro(const std::string& registroBusca) {
        std::string sql = "SELECT id, nome, registro_conselho, nivel_acesso FROM profissionais WHERE registro_conselho = ?;";
        sqlite3_stmt* stmt;
        Profissional encontrado; // Inicia com id = 0

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, registroBusca.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                encontrado = extrairDaLinha(stmt);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Buscar pelo ID
    Profissional buscarPorId(int idBusca) {
        std::string sql = "SELECT id, nome, registro_conselho, nivel_acesso FROM profissionais WHERE id = ?;";
        sqlite3_stmt* stmt;
        Profissional encontrado;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, idBusca);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                encontrado = extrairDaLinha(stmt);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Lista todos os profissionais
    std::vector<Profissional> listarTodos() {
        std::vector<Profissional> lista;
        std::string sql = "SELECT id, nome, registro_conselho, nivel_acesso FROM profissionais;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                lista.push_back(extrairDaLinha(stmt));
            }
        } else {
            std::cerr << "[Erro ProfissionalDAO] Falha ao listar profissionais: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_finalize(stmt);
        return lista;
    }

    // Atualiza nome ou nível de acesso
    bool atualizar(const Profissional& prof) {
        std::string sql = "UPDATE profissionais SET nome = ?, nivel_acesso = ? WHERE id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, prof.getNome().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, prof.getNivelAcesso().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, prof.getId());

        bool sucesso = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return sucesso;
    }

    // Remover profissional pelo ID
    bool deletar(int id) {
        std::string sql = "DELETE FROM profissionais WHERE id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int(stmt, 1, id);
        bool sucesso = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return sucesso;
    }
};

#endif