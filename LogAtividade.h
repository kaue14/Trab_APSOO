#ifndef LOGATIVIDADE_H
#define LOGATIVIDADE_H

#include <string>

// RF011 — Uma ação registrada no sistema: quem fez, quando, o quê e sobre o quê
class LogAtividade {
private:
    int id;
    std::string data_hora;          // "aaaa-mm-dd hh:mm:ss", hora local
    int profissional_id;
    std::string nome_profissional;  // vem da tabela profissionais na consulta
    std::string tipo;               // "Acesso", "Paciente", "Atendimento" ou "Usuario"
    std::string acao;               // ex.: "Registrou atendimento"
    std::string detalhe;            // ex.: "Benedita Alves da Silva — atendimento de 14/11/2026: ..."

public:
    LogAtividade() : id(0), profissional_id(0) {}

    LogAtividade(int id, std::string data_hora, int profissional_id, std::string nome_profissional,
                 std::string tipo, std::string acao, std::string detalhe) {
        this->id = id;
        this->data_hora = data_hora;
        this->profissional_id = profissional_id;
        this->nome_profissional = nome_profissional;
        this->tipo = tipo;
        this->acao = acao;
        this->detalhe = detalhe;
    }

    // Get
    int getId() const { return id; }
    std::string getDataHora() const { return data_hora; }
    int getProfissionalId() const { return profissional_id; }
    std::string getNomeProfissional() const { return nome_profissional; }
    std::string getTipo() const { return tipo; }
    std::string getAcao() const { return acao; }
    std::string getDetalhe() const { return detalhe; }
};

#endif
