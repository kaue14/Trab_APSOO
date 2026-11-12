#ifndef PACIENTE_H
#define PACIENTE_H

#include <string>

class Paciente {
private:
    int id;
    std::string nome;
    std::string cpf;
    std::string data_nascimento;
    std::string carteira;           // RF006: o paciente é identificado pela carteira ou pelo CPF

    // Endereço do atendimento domiciliar
    std::string rua;                // logradouro e número
    std::string complemento;
    std::string bairro;
    std::string cidade;
    std::string uf;

public:
    // Construtor vazio
    Paciente() : id(0), nome(""), cpf(""), data_nascimento("") {}

    // Construtor
    Paciente(int id, std::string nome, std::string cpf, std::string data_nascimento = "") {
        this->id = id;
        this->nome = nome;
        this->cpf = cpf;
        this->data_nascimento = data_nascimento;
    }

    // Get
    int getId() const { return id; }
    std::string getNome() const { return nome; }
    std::string getCpf() const { return cpf; }
    std::string getDataNascimento() const { return data_nascimento; }
    std::string getCarteira() const { return carteira; }
    std::string getRua() const { return rua; }
    std::string getComplemento() const { return complemento; }
    std::string getBairro() const { return bairro; }
    std::string getCidade() const { return cidade; }
    std::string getUf() const { return uf; }

    // Set
    void setId(int novoId) { id = novoId; }
    void setNome(const std::string& novoNome) { nome = novoNome; }
    void setDataNascimento(const std::string& novaData) { data_nascimento = novaData; }
    void setCpf(const std::string& novoCpf) { cpf = novoCpf; }
    void setCarteira(const std::string& novaCarteira) { carteira = novaCarteira; }
    void setEndereco(const std::string& novaRua, const std::string& novoComplemento,
                     const std::string& novoBairro, const std::string& novaCidade, const std::string& novaUf) {
        rua = novaRua;
        complemento = novoComplemento;
        bairro = novoBairro;
        cidade = novaCidade;
        uf = novaUf;
    }
};

#endif
