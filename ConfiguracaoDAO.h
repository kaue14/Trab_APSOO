#ifndef CONFIGURACAODAO_H
#define CONFIGURACAODAO_H

#include "Database.h"
#include "DadosGerais.h"
#include <string>
#include <iostream>

// RF012 — Lê e grava os dados gerais na tabela configuracoes (uma linha por informação: chave e valor).
// Assim, uma informação nova no futuro não exige mudar a estrutura do banco.
class ConfiguracaoDAO {
private:
    sqlite3* db;

    std::string ler(const std::string& chave) {
        std::string sql = "SELECT valor FROM configuracoes WHERE chave = ?;";
        sqlite3_stmt* stmt;
        std::string valor;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, chave.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* raw = sqlite3_column_text(stmt, 0);
                if (raw) valor = reinterpret_cast<const char*>(raw);
            }
        }
        sqlite3_finalize(stmt);
        return valor;
    }

    bool gravar(const std::string& chave, const std::string& valor) {
        std::string sql = "INSERT OR REPLACE INTO configuracoes (chave, valor) VALUES (?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, chave.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, valor.c_str(), -1, SQLITE_TRANSIENT);

        bool sucesso = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return sucesso;
    }

public:
    ConfiguracaoDAO(Database& conexao) {
        this->db = conexao.getDb();
    }

    // [READ] Informação ainda não configurada volta vazia
    DadosGerais carregarDadosGerais() {
        DadosGerais dados;
        dados.setNomeEmpresa(ler("empresa_nome"));
        dados.setCnpj(ler("empresa_cnpj"));
        dados.setTelefone(ler("empresa_telefone"));
        dados.setEmail(ler("empresa_email"));
        dados.setCidade(ler("empresa_cidade"));
        dados.setUf(ler("empresa_uf"));
        return dados;
    }

    // [UPDATE] Grava tudo numa transação: ou salva todas as informações, ou nenhuma
    bool salvarDadosGerais(const DadosGerais& dados) {
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        bool sucesso = gravar("empresa_nome", dados.getNomeEmpresa()) &&
                       gravar("empresa_cnpj", dados.getCnpj()) &&
                       gravar("empresa_telefone", dados.getTelefone()) &&
                       gravar("empresa_email", dados.getEmail()) &&
                       gravar("empresa_cidade", dados.getCidade()) &&
                       gravar("empresa_uf", dados.getUf());

        if (!sucesso) {
            std::cerr << "[Erro ConfiguracaoDAO] Falha ao salvar: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        return true;
    }
};

#endif
