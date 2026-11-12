#ifndef PACIENTEDAO_H
#define PACIENTEDAO_H

#include "Database.h"
#include "Paciente.h"
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

class PacienteDAO {
private:
    sqlite3* db;

    // Colunas lidas em todas as consultas, na ordem usada por extrairDaLinha
    const std::string colunas = "id, nome, cpf, data_nascimento, carteira, rua, complemento, bairro, cidade, uf";

    static std::string lerTexto(sqlite3_stmt* stmt, int coluna) {
        const unsigned char* raw = sqlite3_column_text(stmt, coluna);
        return raw ? reinterpret_cast<const char*>(raw) : "";
    }

    // Texto vazio é gravado como NULL, para CPF e carteira vazios não colidirem no UNIQUE
    static void vincularTexto(sqlite3_stmt* stmt, int posicao, const std::string& valor) {
        if (valor.empty()) sqlite3_bind_null(stmt, posicao);
        else sqlite3_bind_text(stmt, posicao, valor.c_str(), -1, SQLITE_TRANSIENT);
    }

    // Método auxiliar interno para converter uma linha do SQLite em objeto Paciente
    Paciente extrairDaLinha(sqlite3_stmt* stmt) {
        Paciente p(sqlite3_column_int(stmt, 0), lerTexto(stmt, 1), lerTexto(stmt, 2), lerTexto(stmt, 3));
        p.setCarteira(lerTexto(stmt, 4));
        p.setEndereco(lerTexto(stmt, 5), lerTexto(stmt, 6), lerTexto(stmt, 7), lerTexto(stmt, 8), lerTexto(stmt, 9));
        return p;
    }

    // Busca um paciente por uma coluna de texto (cpf ou carteira)
    Paciente buscarPor(const std::string& coluna, const std::string& valor) {
        std::string sql = "SELECT " + colunas + " FROM pacientes WHERE " + coluna + " = ?;";
        sqlite3_stmt* stmt;
        Paciente encontrado; // Começa com id = 0

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, valor.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                encontrado = extrairDaLinha(stmt);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Ignora acentos e maiúsculas: "jose" encontra "José" (o LIKE do SQLite não trata acentos)
    static std::string normalizar(const std::string& texto) {
        static const std::pair<const char*, char> acentos[] = {
            {"á", 'a'}, {"à", 'a'}, {"â", 'a'}, {"ã", 'a'}, {"ä", 'a'}, {"Á", 'a'}, {"À", 'a'}, {"Â", 'a'}, {"Ã", 'a'}, {"Ä", 'a'},
            {"é", 'e'}, {"è", 'e'}, {"ê", 'e'}, {"ë", 'e'}, {"É", 'e'}, {"È", 'e'}, {"Ê", 'e'}, {"Ë", 'e'},
            {"í", 'i'}, {"ì", 'i'}, {"î", 'i'}, {"ï", 'i'}, {"Í", 'i'}, {"Ì", 'i'}, {"Î", 'i'}, {"Ï", 'i'},
            {"ó", 'o'}, {"ò", 'o'}, {"ô", 'o'}, {"õ", 'o'}, {"ö", 'o'}, {"Ó", 'o'}, {"Ò", 'o'}, {"Ô", 'o'}, {"Õ", 'o'}, {"Ö", 'o'},
            {"ú", 'u'}, {"ù", 'u'}, {"û", 'u'}, {"ü", 'u'}, {"Ú", 'u'}, {"Ù", 'u'}, {"Û", 'u'}, {"Ü", 'u'},
            {"ç", 'c'}, {"Ç", 'c'},
        };
        std::string resultado;
        for (size_t i = 0; i < texto.size();) {
            bool trocou = false;
            for (const auto& acento : acentos) {
                const size_t tamanho = std::strlen(acento.first);
                if (texto.compare(i, tamanho, acento.first) == 0) {
                    resultado += acento.second;
                    i += tamanho;
                    trocou = true;
                    break;
                }
            }
            if (!trocou) resultado += static_cast<char>(std::tolower(static_cast<unsigned char>(texto[i++])));
        }
        return resultado;
    }

    static std::string apenasDigitos(const std::string& texto) {
        std::string digitos;
        for (char c : texto) {
            if (std::isdigit(static_cast<unsigned char>(c))) digitos += c;
        }
        return digitos;
    }

public:
    PacienteDAO(Database& conexao) {
        this->db = conexao.getDb();
    }

    // Cadastrar paciente RF006
    bool inserir(Paciente& p) {
        std::string sql = "INSERT INTO pacientes (nome, cpf, data_nascimento, carteira, rua, complemento, bairro, cidade, uf) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro DAO] Falha ao preparar INSERT: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, p.getNome().c_str(), -1, SQLITE_TRANSIENT);
        vincularTexto(stmt, 2, p.getCpf());
        sqlite3_bind_text(stmt, 3, p.getDataNascimento().c_str(), -1, SQLITE_TRANSIENT);
        vincularTexto(stmt, 4, p.getCarteira());
        vincularTexto(stmt, 5, p.getRua());
        vincularTexto(stmt, 6, p.getComplemento());
        vincularTexto(stmt, 7, p.getBairro());
        vincularTexto(stmt, 8, p.getCidade());
        vincularTexto(stmt, 9, p.getUf());

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

    // Listar todos os pacientes, em ordem alfabética
    std::vector<Paciente> listarTodos() {
        std::vector<Paciente> lista;
        std::string sql = "SELECT " + colunas + " FROM pacientes ORDER BY nome COLLATE NOCASE;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro DAO] Falha ao listar pacientes: " << sqlite3_errmsg(db) << std::endl;
            return lista;
        }

        // Passa linha por linha da tabela
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            lista.push_back(extrairDaLinha(stmt));
        }

        sqlite3_finalize(stmt);
        return lista;
    }

    // Buscar paciente por nome ou número de identificação (CPF ou carteira) RF007
    std::vector<Paciente> buscarPorTermo(const std::string& termo) {
        std::vector<Paciente> encontrados;
        const std::string nomeBusca = normalizar(termo);
        const std::string numeroBusca = apenasDigitos(termo);

        for (const auto& p : listarTodos()) {
            bool nomeConfere = !nomeBusca.empty() && normalizar(p.getNome()).find(nomeBusca) != std::string::npos;
            bool numeroConfere = numeroBusca.size() >= 3 &&
                (apenasDigitos(p.getCpf()).find(numeroBusca) != std::string::npos ||
                 apenasDigitos(p.getCarteira()).find(numeroBusca) != std::string::npos);
            if (nomeConfere || numeroConfere) encontrados.push_back(p);
        }
        return encontrados;
    }

    // Buscar paciente por CPF
    Paciente buscarPorCpf(const std::string& cpfBusca) {
        return buscarPor("cpf", cpfBusca);
    }

    // Buscar paciente pelo número da carteira
    Paciente buscarPorCarteira(const std::string& carteiraBusca) {
        return buscarPor("carteira", carteiraBusca);
    }

    // Buscar pelo ID
    Paciente buscarPorId(int idBusca) {
        std::string sql = "SELECT " + colunas + " FROM pacientes WHERE id = ?;";
        sqlite3_stmt* stmt;
        Paciente encontrado;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, idBusca);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                encontrado = extrairDaLinha(stmt);
            }
        }
        sqlite3_finalize(stmt);
        return encontrado;
    }

    // Editar dados do paciente RF008
    bool atualizar(const Paciente& p) {
        std::string sql = "UPDATE pacientes SET nome = ?, cpf = ?, data_nascimento = ?, carteira = ?, "
                          "rua = ?, complemento = ?, bairro = ?, cidade = ?, uf = ? WHERE id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[Erro DAO] Falha ao preparar UPDATE: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, p.getNome().c_str(), -1, SQLITE_TRANSIENT);
        vincularTexto(stmt, 2, p.getCpf());
        sqlite3_bind_text(stmt, 3, p.getDataNascimento().c_str(), -1, SQLITE_TRANSIENT);
        vincularTexto(stmt, 4, p.getCarteira());
        vincularTexto(stmt, 5, p.getRua());
        vincularTexto(stmt, 6, p.getComplemento());
        vincularTexto(stmt, 7, p.getBairro());
        vincularTexto(stmt, 8, p.getCidade());
        vincularTexto(stmt, 9, p.getUf());
        sqlite3_bind_int(stmt, 10, p.getId());

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
