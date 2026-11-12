#ifndef PROFISSIONAL_H
#define PROFISSIONAL_H

#include <string>

class Profissional {
private:
    int id;
    std::string nome;
    std::string registro_conselho;
    std::string nivel_acesso;      // "Tecnico", "Enfermeiro" ou "Administrador"

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

    // Set
    void setId(int novoId) { id = novoId; }
    void setNome(const std::string& novoNome) { nome = novoNome; }
    void setNivelAcesso(const std::string& novoNivel) {
        if (novoNivel == "Tecnico" || novoNivel == "Enfermeiro" || novoNivel == "Administrador") {
            nivel_acesso = novoNivel;
        }
    }

    // Checa nivel de acesso
    bool isEnfermeiroOuSuperior() const {
        return (nivel_acesso == "Enfermeiro" || nivel_acesso == "Administrador");
    }
    bool isAdministrador() const {
        return (nivel_acesso == "Administrador");
    }
};

#endif