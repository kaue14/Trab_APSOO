#ifndef PACIENTE_H
#define PACIENTE_H

#include <string>

class Paciente {
private:
    int id;
    std::string nome;
    std::string cpf;
    std::string data_nascimento;

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

    // Set
    void setId(int novoId) { id = novoId; }
    void setNome(const std::string& novoNome) { nome = novoNome; }
    void setDataNascimento(const std::string& novaData) { data_nascimento = novaData; }
};

#endif