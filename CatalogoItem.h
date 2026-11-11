#ifndef CATALOGOITEM_H
#define CATALOGOITEM_H

#include <string>

class CatalogoItem {
private:
    int id;
    std::string codigo;
    std::string descricao;
    double valor;
    std::string tipo;

public:
    // Construtor vazio
    CatalogoItem() : id(0), codigo(""), descricao(""), valor(0.0), tipo("") {}

    CatalogoItem(int id, std::string codigo, std::string descricao, double valor, std::string tipo) {
        this->id = id;
        this->codigo = codigo;
        this->descricao = descricao;
        this->valor = valor;
        this->tipo = tipo;
    }

    // Get
    int getId() const { return id; }
    std::string getCodigo() const { return codigo; }
    std::string getDescricao() const { return descricao; }
    double getValor() const { return valor; }
    std::string getTipo() const { return tipo; }

    // Set
    void setId(int novoId) { id = novoId; }
    void setValor(double novoValor) {valor = novoValor; }
    void setDescricao(const std::string& novaDesc) {descricao = novaDesc; }
};

#endif