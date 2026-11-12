#ifndef ATENDIMENTO_H
#define ATENDIMENTO_H

#include "AtendimentoItem.h"
#include <string>
#include <vector>

class Atendimento {
private:
    int id;
    int paciente_id;
    int profissional_id;
    std::string data_atendimento;
    std::vector<AtendimentoItem> itens;

public:
    // Construtor vazio
    Atendimento() : id(0), paciente_id(0), profissional_id(0), data_atendimento("") {}

    // Construtor
    Atendimento(int id, int paciente_id, int profissional_id, std::string data_atendimento)
        : id(id), paciente_id(paciente_id), profissional_id(profissional_id), data_atendimento(data_atendimento) {}

    // Adiciona um procedimento ou material (RF009 / RF010)
    void adicionarItem(int catalogo_id, int quantidade) {
        itens.push_back(AtendimentoItem(catalogo_id, quantidade));
    }
    void adicionarItemCompleto(const AtendimentoItem& item) {
        itens.push_back(item);
    }

    // Calcula o total da guia somando todos os subtotais dos itens
    double calcularTotalGuia() const {
        double total = 0.0;
        for (const auto& item : itens) {
            total += item.getSubtotal();
        }
        return total;
    }

    // Get
    int getId() const { return id; }
    int getPacienteId() const { return paciente_id; }
    int getProfissionalId() const { return profissional_id; }
    std::string getDataAtendimento() const { return data_atendimento; }
    const std::vector<AtendimentoItem>& getItens() const { return itens; }

    // Set
    void setId(int novoId) { id = novoId; }
    void setDataAtendimento(const std::string& novaData) { data_atendimento = novaData; }
};

#endif