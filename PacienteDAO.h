#ifndef PACIENTEDAO_H
#define PACIENTEDAO_H

#include "Database.h"
#include "Paciente.h"
#include <string>
#include <vector>
#include <iostream>

class PacienteDAO {
private:
    sqlite3* db;

public:
    PacienteDAO(Database& conexao) {
        this->db = conexao.getDb();
    }

    // Cadastrar paciente RF006
    bool inserir(Paciente& p) {
        std::string sql = "INSERT INTO pacientes (nome, cpf, data_nascimento) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro DAO] Falha ao preparar INSERT: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, p.getNome().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, p.getCpf().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, p.getDataNascimento().c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "[Erro DAO] Falha ao inserir paciente: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        // Salva o ID do paciente
        p.setId((int)sqlite3_last_insert_rowid(db));

        sqlite3_finalize(stmt);
        std::cout << "[DAO] Paciente '" << p.getNome() << "' cadastrado com ID " << p.getId() << "!" << std::endl;
        return true;
    }

    // Listar todos os pacientes
    std::vector<Paciente> listarTodos() {
        std::vector<Paciente> lista;
        std::string sql = "SELECT id, nome, cpf, data_nascimento FROM pacientes;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro DAO] Falha ao listar pacientes: " << sqlite3_errmsg(db) << std::endl;
            return lista;
        }

        // Passa linha por linha da tabela
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            std::string nome = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string cpf = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            
            const unsigned char* dataRaw = sqlite3_column_text(stmt, 3);
            std::string dataNasc = dataRaw ? reinterpret_cast<const char*>(dataRaw) : "";

            lista.push_back(Paciente(id, nome, cpf, dataNasc));
        }

        sqlite3_finalize(stmt);
        return lista;
    }

    // Buscar paciente por CPF RF007
    Paciente buscarPorCpf(const std::string& cpfBusca) {
        std::string sql = "SELECT id, nome, cpf, data_nascimento FROM pacientes WHERE cpf = ?;";
        sqlite3_stmt* stmt;
        Paciente encontrado; // Começa com id = 0

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, cpfBusca.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                std::string nome = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                std::string cpf = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                
                const unsigned char* dataRaw = sqlite3_column_text(stmt, 3);
                std::string dataNasc = dataRaw ? reinterpret_cast<const char*>(dataRaw) : "";

                encontrado = Paciente(id, nome, cpf, dataNasc);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Editar dados do paciente RF008
    bool atualizar(const Paciente& p) {
        std::string sql = "UPDATE pacientes SET nome = ?, data_nascimento = ? WHERE id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro DAO] Falha ao preparar UPDATE: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, p.getNome().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, p.getDataNascimento().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, p.getId());

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "[Erro DAO] Falha ao atualizar paciente: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        std::cout << "[DAO] Paciente ID " << p.getId() << " atualizado com sucesso!" << std::endl;
        return true;
    }

    // Remover paciente pelo ID
    bool deletar(int id) {
        std::string sql = "DELETE FROM pacientes WHERE id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro DAO] Falha ao preparar DELETE: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "[Erro DAO] Falha ao deletar paciente: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        std::cout << "[DAO] Paciente ID " << id << " removido do banco!" << std::endl;
        return true;
    }
};

#endif