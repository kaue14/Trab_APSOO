#ifndef ATENDIMENTODAO_H
#define ATENDIMENTODAO_H

#include "Database.h"
#include "Atendimento.h"
#include <string>
#include <vector>
#include <iostream>

class AtendimentoDAO {
private:
    sqlite3* db;

    // Carrega todos os materiais/procedimentos do  atendimento e faz JOIN com o catálogo
    void carregarItensDoAtendimento(Atendimento& atendimento) {
        std::string sql = "SELECT ai.id, ai.atendimento_id, ai.catalogo_id, ai.quantidade, "
                          "c.codigo, c.descricao, c.valor "
                          "FROM atendimento_itens ai "
                          "INNER JOIN catalogo_itens c ON ai.catalogo_id = c.id "
                          "WHERE ai.atendimento_id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, atendimento.getId());

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                int atendId = sqlite3_column_int(stmt, 1);
                int catId = sqlite3_column_int(stmt, 2);
                int qtd = sqlite3_column_int(stmt, 3);

                const unsigned char* codRaw = sqlite3_column_text(stmt, 4);
                std::string codigo = codRaw ? reinterpret_cast<const char*>(codRaw) : "";

                const unsigned char* descRaw = sqlite3_column_text(stmt, 5);
                std::string descricao = descRaw ? reinterpret_cast<const char*>(descRaw) : "";

                double valor = sqlite3_column_double(stmt, 6);

                atendimento.adicionarItemCompleto(
                    AtendimentoItem(id, atendId, catId, qtd, codigo, descricao, valor)
                );
            }
        }
        sqlite3_finalize(stmt);
    }

public:
    AtendimentoDAO(Database& conexao) {
        this->db = conexao.getDb();
    }

    // Registra o Atendimento e todos os procedimentos/materiais em uma transação RF009 e RF010
    bool registrarAtendimento(Atendimento& atendimento) {
        char* erroMsg = nullptr;
        
        // Inicia transação para garantir que cabeçalho e itens sejam salvos juntos
        // Inicia transaction para salvar cabeçalho e itens ao mesmo tempo
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &erroMsg);

        std::string sqlAtend = "INSERT INTO atendimentos (paciente_id, profissional_id, data_atendimento) VALUES (?, ?, ?);";
        sqlite3_stmt* stmtAtend;

        if (sqlite3_prepare_v2(db, sqlAtend.c_str(), -1, &stmtAtend, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro AtendimentoDAO] Falha ao preparar guia: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        sqlite3_bind_int(stmtAtend, 1, atendimento.getPacienteId());
        sqlite3_bind_int(stmtAtend, 2, atendimento.getProfissionalId());
        sqlite3_bind_text(stmtAtend, 3, atendimento.getDataAtendimento().c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmtAtend) != SQLITE_DONE) {
            std::cerr << "[Erro AtendimentoDAO] Falha ao inserir guia: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmtAtend);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        int novoAtendimentoId = (int)sqlite3_last_insert_rowid(db);
        atendimento.setId(novoAtendimentoId);
        sqlite3_finalize(stmtAtend);

        // Insere os itens na tabela atendimento_itens
        std::string sqlItem = "INSERT INTO atendimento_itens (atendimento_id, catalogo_id, quantidade) VALUES (?, ?, ?);";
        for (const auto& item : atendimento.getItens()) {
            sqlite3_stmt* stmtItem;
            if (sqlite3_prepare_v2(db, sqlItem.c_str(), -1, &stmtItem, nullptr) != SQLITE_OK) {
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                return false;
            }

            sqlite3_bind_int(stmtItem, 1, novoAtendimentoId);
            sqlite3_bind_int(stmtItem, 2, item.getCatalogoId());
            sqlite3_bind_int(stmtItem, 3, item.getQuantidade());

            if (sqlite3_step(stmtItem) != SQLITE_DONE) {
                std::cerr << "[Erro AtendimentoDAO] Falha ao inserir item da guia: " << sqlite3_errmsg(db) << std::endl;
                sqlite3_finalize(stmtItem);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                return false;
            }
            sqlite3_finalize(stmtItem);
        }

        // Salva no arquivo homecare.db
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &erroMsg);
        std::cout << "[DAO] Guia de Atendimento ID " << novoAtendimentoId << " registrada com " 
                  << atendimento.getItens().size() << " item(ns)!" << std::endl;
        return true;
    }

    // Busca guia com todos os itens e preços
    Atendimento buscarPorId(int idAtendimento) {
        std::string sql = "SELECT id, paciente_id, profissional_id, data_atendimento FROM atendimentos WHERE id = ?;";
        sqlite3_stmt* stmt;
        Atendimento encontrado;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, idAtendimento);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                int pacId = sqlite3_column_int(stmt, 1);
                int profId = sqlite3_column_int(stmt, 2);
                
                const unsigned char* dataRaw = sqlite3_column_text(stmt, 3);
                std::string data = dataRaw ? reinterpret_cast<const char*>(dataRaw) : "";

                encontrado = Atendimento(id, pacId, profId, data);
                carregarItensDoAtendimento(encontrado);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Lista todas as guias de um mês (ex: mesAno = "2026-10")
    std::vector<Atendimento> listarPorMes(const std::string& mesAno) {
        std::vector<Atendimento> lista;
        std::string sql = "SELECT id, paciente_id, profissional_id, data_atendimento "
                          "FROM atendimentos WHERE data_atendimento LIKE ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            std::string filtro = mesAno + "%"; // Ex: "2026-10%" pega todos os dias de outubro de 2026
            sqlite3_bind_text(stmt, 1, filtro.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                int pacId = sqlite3_column_int(stmt, 1);
                int profId = sqlite3_column_int(stmt, 2);
                
                const unsigned char* dataRaw = sqlite3_column_text(stmt, 3);
                std::string data = dataRaw ? reinterpret_cast<const char*>(dataRaw) : "";

                Atendimento atend(id, pacId, profId, data);
                carregarItensDoAtendimento(atend);
                lista.push_back(atend);
            }
        }
        sqlite3_finalize(stmt);
        return lista;
    }

    // Editar procedimento/material RF012
    bool atualizarItemRegistrado(int atendimentoItemId, int novoCatalogoId, int novaQuantidade) {
        std::string sql = "UPDATE atendimento_itens SET catalogo_id = ?, quantidade = ? WHERE id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int(stmt, 1, novoCatalogoId);
        sqlite3_bind_int(stmt, 2, novaQuantidade);
        sqlite3_bind_int(stmt, 3, atendimentoItemId);

        bool sucesso = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return sucesso;
    }

    // Consultar fechamento mensal RF011
    double calcularFechamentoMensal(const std::string& mesAno) {
        std::string sql = "SELECT SUM(ai.quantidade * c.valor) "
                          "FROM atendimento_itens ai "
                          "INNER JOIN atendimentos a ON ai.atendimento_id = a.id "
                          "INNER JOIN catalogo_itens c ON ai.catalogo_id = c.id "
                          "WHERE a.data_atendimento LIKE ?;";
        sqlite3_stmt* stmt;
        double totalMensal = 0.0;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            std::string filtro = mesAno + "%";
            sqlite3_bind_text(stmt, 1, filtro.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                totalMensal = sqlite3_column_double(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
        return totalMensal;
    }

    // Remove a guia e seus itens
    bool deletarAtendimento(int atendimentoId) {
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        // Deleta primeiro os itens filhos da guia
        std::string sqlItens = "DELETE FROM atendimento_itens WHERE atendimento_id = ?;";
        sqlite3_stmt* stmtItens;
        if (sqlite3_prepare_v2(db, sqlItens.c_str(), -1, &stmtItens, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmtItens, 1, atendimentoId);
            sqlite3_step(stmtItens);
        }
        sqlite3_finalize(stmtItens);

        // Deleta o cabeçalho da guia
        std::string sqlAtend = "DELETE FROM atendimentos WHERE id = ?;";
        sqlite3_stmt* stmtAtend;
        bool sucesso = false;
        if (sqlite3_prepare_v2(db, sqlAtend.c_str(), -1, &stmtAtend, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmtAtend, 1, atendimentoId);
            sucesso = (sqlite3_step(stmtAtend) == SQLITE_DONE);
        }
        sqlite3_finalize(stmtAtend);

        sqlite3_exec(db, sucesso ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, nullptr);
        return sucesso;
    }
};

#endif