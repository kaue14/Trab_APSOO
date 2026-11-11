#ifndef CATALOGODAO_H
#define CATALOGODAO_H

#include "Database.h"
#include "CatalogoItem.h"
#include <string>
#include <vector>
#include <iostream>

class CatalogoDAO {
private:
    sqlite3* db;

    // Método auxiliar interno para converter uma linha do SQLite em objeto CatalogoItem
    CatalogoItem extrairItemDaLinha(sqlite3_stmt* stmt) {
        int id = sqlite3_column_int(stmt, 0);
        
        const unsigned char* codRaw = sqlite3_column_text(stmt, 1);
        std::string codigo = codRaw ? reinterpret_cast<const char*>(codRaw) : "";

        const unsigned char* descRaw = sqlite3_column_text(stmt, 2);
        std::string descricao = descRaw ? reinterpret_cast<const char*>(descRaw) : "";

        double valor = sqlite3_column_double(stmt, 3);

        const unsigned char* tipoRaw = sqlite3_column_text(stmt, 4);
        std::string tipo = tipoRaw ? reinterpret_cast<const char*>(tipoRaw) : "";

        return CatalogoItem(id, codigo, descricao, valor, tipo);
    }

public:
    CatalogoDAO(Database& conexao) {
        this->db = conexao.getDb();
    }

    // Lista todos os itens do catálogo
    std::vector<CatalogoItem> listarTodos() {
        std::vector<CatalogoItem> lista;
        std::string sql = "SELECT id, codigo, descricao, valor, tipo FROM catalogo_itens;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                lista.push_back(extrairItemDaLinha(stmt));
            }
        } else {
            std::cerr << "[Erro CatalogoDAO] Falha ao listar catalogo: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_finalize(stmt);
        return lista;
    }

    // Filtra apenas "Procedimento", "Material" ou "Kit" (RF009 e RF010)
    std::vector<CatalogoItem> listarPorTipo(const std::string& tipoFiltro) {
        std::vector<CatalogoItem> lista;
        std::string sql = "SELECT id, codigo, descricao, valor, tipo FROM catalogo_itens WHERE tipo = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, tipoFiltro.c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                lista.push_back(extrairItemDaLinha(stmt));
            }
        }
        sqlite3_finalize(stmt);
        return lista;
    }

    // Busca por nome/descrição (Ex: "Seringa", "Curativo")
    std::vector<CatalogoItem> buscarPorTermo(const std::string& termo) {
        std::vector<CatalogoItem> lista;
        std::string sql = "SELECT id, codigo, descricao, valor, tipo FROM catalogo_itens WHERE descricao LIKE ? OR codigo LIKE ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            std::string padrao = "%" + termo + "%";
            sqlite3_bind_text(stmt, 1, padrao.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, padrao.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                lista.push_back(extrairItemDaLinha(stmt));
            }
        }
        sqlite3_finalize(stmt);
        return lista;
    }

    // Busca pelo código
    CatalogoItem buscarPorCodigo(const std::string& codigoBusca) {
        std::string sql = "SELECT id, codigo, descricao, valor, tipo FROM catalogo_itens WHERE codigo = ?;";
        sqlite3_stmt* stmt;
        CatalogoItem encontrado;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, codigoBusca.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                encontrado = extrairItemDaLinha(stmt);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Busca por ID
    CatalogoItem buscarPorId(int idBusca) {
        std::string sql = "SELECT id, codigo, descricao, valor, tipo FROM catalogo_itens WHERE id = ?;";
        sqlite3_stmt* stmt;
        CatalogoItem encontrado;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, idBusca);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                encontrado = extrairItemDaLinha(stmt);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Cadastra 1 item
    bool inserir(CatalogoItem& item) {
        std::string sql = "INSERT INTO catalogo_itens (codigo, descricao, valor, tipo) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro CatalogoDAO] Falha ao preparar INSERT: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, item.getCodigo().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, item.getDescricao().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, item.getValor());
        sqlite3_bind_text(stmt, 4, item.getTipo().c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "[Erro CatalogoDAO] Falha ao inserir item: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        item.setId((int)sqlite3_last_insert_rowid(db));
        sqlite3_finalize(stmt);
        return true;
    }

    // Atuzlizar preço do item
    bool atualizarValor(int id, double novoValor) {
        std::string sql = "UPDATE catalogo_itens SET valor = ? WHERE id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_double(stmt, 1, novoValor);
        sqlite3_bind_int(stmt, 2, id);

        bool sucesso = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return sucesso;
    }
};

#endif