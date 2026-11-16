#ifndef DADOSGERAIS_H
#define DADOSGERAIS_H

#include <string>

// RF014 — Dados gerais da empresa de home care (tela Configurações)
class DadosGerais {
private:
    std::string nome_empresa;
    std::string cnpj;
    std::string telefone;
    std::string email;
    std::string cidade;
    std::string uf;

public:
    DadosGerais() {}

    // Get
    std::string getNomeEmpresa() const { return nome_empresa; }
    std::string getCnpj() const { return cnpj; }
    std::string getTelefone() const { return telefone; }
    std::string getEmail() const { return email; }
    std::string getCidade() const { return cidade; }
    std::string getUf() const { return uf; }

    // Set
    void setNomeEmpresa(const std::string& valor) { nome_empresa = valor; }
    void setCnpj(const std::string& valor) { cnpj = valor; }
    void setTelefone(const std::string& valor) { telefone = valor; }
    void setEmail(const std::string& valor) { email = valor; }
    void setCidade(const std::string& valor) { cidade = valor; }
    void setUf(const std::string& valor) { uf = valor; }
};

#endif
