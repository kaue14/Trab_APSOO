#ifndef ATENDIMENTOITEM_H
#define ATENDIMENTOITEM_H

#include <string>

class AtendimentoItem {
private:
    int id;
    int atendimento_id;
    int catalogo_id;
    int quantidade;

    // Campos auxiliares populados via JOIN com catalogo_itens para exibição e cálculo
    std::string codigo_item;
    std::string descricao_item;
    double valor_unitario;

public:
    // Construtor vazio
    AtendimentoItem() 
        : id(0), atendimento_id(0), catalogo_id(0), quantidade(0), 
          codigo_item(""), descricao_item(""), valor_unitario(0.0) {}

    // Construtor para adicionar 1 item
    AtendimentoItem(int catalogo_id, int quantidade)
        : id(0), atendimento_id(0), catalogo_id(catalogo_id), quantidade(quantidade),
          codigo_item(""), descricao_item(""), valor_unitario(0.0) {}

    // Construtor
    AtendimentoItem(int id, int atendimento_id, int catalogo_id, int quantidade,
                    std::string codigo, std::string descricao, double valor)
        : id(id), atendimento_id(atendimento_id), catalogo_id(catalogo_id), quantidade(quantidade),
          codigo_item(codigo), descricao_item(descricao), valor_unitario(valor) {}

    // Get
    int getId() const { return id; }
    int getAtendimentoId() const { return atendimento_id; }
    int getCatalogoId() const { return catalogo_id; }
    int getQuantidade() const { return quantidade; }
    std::string getCodigoItem() const { return codigo_item; }
    std::string getDescricaoItem() const { return descricao_item; }
    double getValorUnitario() const { return valor_unitario; }
    double getSubtotal() const { return quantidade * valor_unitario; }

    // Set
    void setId(int novoId) { id = novoId; }
    void setAtendimentoId(int idAtend) { atendimento_id = idAtend; }
    void setQuantidade(int novaQtd) { quantidade = novaQtd; }
};

#endif