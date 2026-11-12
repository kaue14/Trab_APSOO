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

    // Colunas lidas em todas as consultas, na ordem usada por extrairDaLinha
    const std::string colunas = "id, nome, registro_conselho, nivel_acesso, login, cpf, senha_hash";

    static std::string lerTexto(sqlite3_stmt* stmt, int coluna) {
        const unsigned char* raw = sqlite3_column_text(stmt, coluna);
        return raw ? reinterpret_cast<const char*>(raw) : "";
    }

    // Texto vazio é gravado como NULL, para login e CPF vazios não colidirem no índice UNIQUE
    static void vincularTexto(sqlite3_stmt* stmt, int posicao, const std::string& valor) {
        if (valor.empty()) sqlite3_bind_null(stmt, posicao);
        else sqlite3_bind_text(stmt, posicao, valor.c_str(), -1, SQLITE_TRANSIENT);
    }

    // Método auxiliar interno para converter uma linha do SQLite em objeto Profissional
    Profissional extrairDaLinha(sqlite3_stmt* stmt) {
        Profissional p(sqlite3_column_int(stmt, 0), lerTexto(stmt, 1), lerTexto(stmt, 2), lerTexto(stmt, 3));
        p.setLogin(lerTexto(stmt, 4));
        p.setCpf(lerTexto(stmt, 5));
        p.setSenhaHash(lerTexto(stmt, 6));
        return p;
    }

public:
    ProfissionalDAO(Database& conexao) {
        this->db = conexao.getDb();
    }

    // [CREATE] RF002 - Cadastrar usuário (Técnico, Enfermeiro ou Administrador)
    bool inserir(Profissional& prof) {
        std::string sql = "INSERT INTO profissionais (nome, registro_conselho, nivel_acesso, login, cpf, senha_hash) "
                          "VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro ProfissionalDAO] Falha ao preparar INSERT: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, prof.getNome().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, prof.getRegistroConselho().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, prof.getNivelAcesso().c_str(), -1, SQLITE_TRANSIENT);
        vincularTexto(stmt, 4, prof.getLogin());
        vincularTexto(stmt, 5, prof.getCpf());
        vincularTexto(stmt, 6, prof.getSenhaHash());

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

    // Buscar pelo registro do conselho (COREN)
    Profissional buscarPorRegistro(const std::string& registroBusca) {
        std::string sql = "SELECT " + colunas + " FROM profissionais WHERE registro_conselho = ?;";
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

    // Autenticar usuário RF001 — o usuário informa o login ou o CPF (com ou sem pontuação)
    Profissional buscarPorLoginOuCpf(const std::string& loginOuCpf, const std::string& cpfSoDigitos) {
        std::string sql = "SELECT " + colunas + " FROM profissionais "
                          "WHERE login = ? OR (? <> '' AND REPLACE(REPLACE(cpf, '.', ''), '-', '') = ?);";
        sqlite3_stmt* stmt;
        Profissional encontrado;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, loginOuCpf.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, cpfSoDigitos.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, cpfSoDigitos.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                encontrado = extrairDaLinha(stmt);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Buscar pelo ID
    Profissional buscarPorId(int idBusca) {
        std::string sql = "SELECT " + colunas + " FROM profissionais WHERE id = ?;";
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
        std::string sql = "SELECT " + colunas + " FROM profissionais ORDER BY nome COLLATE NOCASE;";
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

    // Define o login e a senha (já em hash) de um profissional existente
    bool definirAcesso(int id, const std::string& novoLogin, const std::string& novoSenhaHash) {
        std::string sql = "UPDATE profissionais SET login = ?, senha_hash = ? WHERE id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        vincularTexto(stmt, 1, novoLogin);
        vincularTexto(stmt, 2, novoSenhaHash);
        sqlite3_bind_int(stmt, 3, id);

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
