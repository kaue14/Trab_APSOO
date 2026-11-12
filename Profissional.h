#ifndef PROFISSIONAL_H
#define PROFISSIONAL_H

#include <string>

class Profissional {
private:
    int id;
    std::string nome;
    std::string registro_conselho;
    std::string nivel_acesso;      // "Tecnico", "Enfermeiro" ou "Administrador"
    std::string login;             // RF001/RF002: entra com o login ou o CPF
    std::string cpf;
    std::string senha_hash;        // NF004: só o hash SHA-256 da senha, nunca a senha

public:
    // Construtor vazio
    Profissional() : id(0), nome(""), registro_conselho(""), nivel_acesso("") {}

    // Construtor
    Profissional(int id, std::string nome, std::string registro_conselho, std::string nivel_acesso) {
        this->id = id;
        this->nome = nome;
        this->registro_conselho = registro_conselho;
        this->nivel_acesso = nivel_acesso;
    }

    // Get
    int getId() const { return id; }
    std::string getNome() const { return nome; }
    std::string getRegistroConselho() const { return registro_conselho; }
    std::string getNivelAcesso() const { return nivel_acesso; }
    std::string getLogin() const { return login; }
    std::string getCpf() const { return cpf; }
    std::string getSenhaHash() const { return senha_hash; }

    // Set
    void setId(int novoId) { id = novoId; }
    void setNome(const std::string& novoNome) { nome = novoNome; }
    void setNivelAcesso(const std::string& novoNivel) {
        if (novoNivel == "Tecnico" || novoNivel == "Enfermeiro" || novoNivel == "Administrador") {
            nivel_acesso = novoNivel;
        }
    }
    void setLogin(const std::string& novoLogin) { login = novoLogin; }
    void setCpf(const std::string& novoCpf) { cpf = novoCpf; }
    void setSenhaHash(const std::string& novoHash) { senha_hash = novoHash; }

    // Checa nivel de acesso
    bool isEnfermeiroOuSuperior() const {
        return (nivel_acesso == "Enfermeiro" || nivel_acesso == "Administrador");
    }
    bool isAdministrador() const {
        return (nivel_acesso == "Administrador");
    }
};

#endif
