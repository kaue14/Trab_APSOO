#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <windows.h>

#include "Database.h"
#include "PacienteDAO.h"
#include "ProfissionalDAO.h"
#include "CatalogoDAO.h"
#include "AtendimentoDAO.h"

// Cadastrar Paciente
void telaCadastrarPaciente(PacienteDAO& pacDao) {
    std::string nome, cpf, dataNasc;

    std::cout << "\n--- CADASTRAR PACIENTE ---" << std::endl;
    std::cout << "Nome completo: ";
    std::getline(std::cin >> std::ws, nome);

    std::cout << "CPF (ex: 111.222.333-44): ";
    std::getline(std::cin >> std::ws, cpf);

    // Verifica se existe paciente com esse CPF
    if (pacDao.buscarPorCpf(cpf).getId() != 0) {
        std::cout << "[Aviso] Ja existe um paciente cadastrado com este CPF!\n";
        return;
    }

    std::cout << "Data de Nascimento (AAAA-MM-DD): ";
    std::getline(std::cin >> std::ws, dataNasc);

    Paciente novo(0, nome, cpf, dataNasc);
    pacDao.inserir(novo);
}

// Buscar paciente
void telaBuscarPaciente(PacienteDAO& pacDao) {
    std::string cpf;
    std::cout << "\n--- BUSCAR PACIENTE ---" << std::endl;
    std::cout << "Digite o CPF: ";
    std::getline(std::cin >> std::ws, cpf);

    Paciente p = pacDao.buscarPorCpf(cpf);
    if (p.getId() != 0) {
        std::cout << "-> ID: " << p.getId() 
                  << " | Nome: " << p.getNome() 
                  << " | Nasc: " << p.getDataNascimento() << std::endl;
    } else {
        std::cout << "-> Nenhum paciente encontrado com o CPF informado." << std::endl;
    }
}

// Registrar procedimento e material
void telaRegistrarAtendimento(PacienteDAO& pacDao, CatalogoDAO& catDao, AtendimentoDAO& atendDao, const Profissional& profLogado) {
    std::string cpfPaciente, dataAtend, termoBusca;

    std::cout << "\n--- REGISTRAR ATENDIMENTO (PROCEDIMENTOS / MATERIAIS) ---" << std::endl;
    std::cout << "CPF do Paciente atendido: ";
    std::getline(std::cin >> std::ws, cpfPaciente);

    Paciente pac = pacDao.buscarPorCpf(cpfPaciente);
    if (pac.getId() == 0) {
        std::cout << "[Erro] Paciente nao encontrado. Cadastre-o primeiro." << std::endl;
        return;
    }

    std::cout << "Paciente selecionado: " << pac.getNome() << std::endl;
    std::cout << "Data do atendimento (AAAA-MM-DD): ";
    std::getline(std::cin >> std::ws, dataAtend);

    Atendimento guia(0, pac.getId(), profLogado.getId(), dataAtend);

    int continuar = 1;
    while (continuar == 1) {
        std::cout << "\nPesquisar item no catalogo (ex: Curativo, Seringa, Consulta): ";
        std::getline(std::cin >> std::ws, termoBusca);

        std::vector<CatalogoItem> encontrados = catDao.buscarPorTermo(termoBusca);
        if (encontrados.empty()) {
            std::cout << "Nenhum item encontrado com esse termo." << std::endl;
            continue;
        }

        for (const auto& item : encontrados) {
            std::cout << "  ID [" << item.getId() << "] " << item.getDescricao() 
                      << " - R$ " << item.getValor() << " (" << item.getTipo() << ")" << std::endl;
        }

        int idEscolhido = 0, qtd = 0;
        std::cout << "Digite o ID do item que deseja adicionar (ou 0 para cancelar): ";
        std::cin >> idEscolhido;

        if (idEscolhido > 0) {
            std::cout << "Quantidade utilizada: ";
            std::cin >> qtd;
            guia.adicionarItem(idEscolhido, qtd);
            std::cout << "-> Item adicionado a guia!" << std::endl;
        }

        std::cout << "Deseja adicionar mais um item a este atendimento? (1 = Sim / 0 = Concluir e Salvar): ";
        std::cin >> continuar;
    }

    if (!guia.getItens().empty()) {
        atendDao.registrarAtendimento(guia);
    } else {
        std::cout << "Atendimento cancelado (nenhum item inserido)." << std::endl;
    }
}

// Fechamento Mensal
void telaFechamentoMensal(AtendimentoDAO& atendDao) {
    std::string mesAno;
    std::cout << "\n--- FECHAMENTO MENSAL ---" << std::endl;
    std::cout << "Digite o Ano e Mes para consulta (formato AAAA-MM, ex: 2026-10): ";
    std::getline(std::cin >> std::ws, mesAno);

    std::vector<Atendimento> guias = atendDao.listarPorMes(mesAno);
    std::cout << "\nGuias encontradas no periodo (" << guias.size() << "):" << std::endl;

    for (const auto& g : guias) {
        std::cout << " - Guia #" << g.getId() << " | Data: " << g.getDataAtendimento()
                  << " | Subtotal da Guia: R$ " << g.calcularTotalGuia() << std::endl;
    }

    double total = atendDao.calcularFechamentoMensal(mesAno);
    std::cout << "==========================================" << std::endl;
    std::cout << "VALOR TOTAL DO FECHAMENTO (" << mesAno << "): R$ " << total << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Cadastrar Usuário
void telaCadastrarUsuario(ProfissionalDAO& profDao, const Profissional& profLogado) {
    if (!profLogado.isAdministrador()) {
        std::cout << "\n[Acesso Negado] Apenas Administradores podem cadastrar novos usuarios." << std::endl;
        return;
    }

    std::string nome, registro, nivel;
    std::cout << "\n--- CADASTRAR NOVO USUARIO ---" << std::endl;
    std::cout << "Nome completo: ";
    std::getline(std::cin >> std::ws, nome);

    std::cout << "Registro do Conselho/Login (ex: COREN-MS 12345): ";
    std::getline(std::cin >> std::ws, registro);

    std::cout << "Nivel de Acesso (Tecnico / Enfermeiro / Administrador): ";
    std::getline(std::cin >> std::ws, nivel);

    Profissional novo(0, nome, registro, nivel);
    profDao.inserir(novo);
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << std::fixed << std::setprecision(2);

    Database db("homecare.db");
    PacienteDAO pacDao(db);
    ProfissionalDAO profDao(db);
    CatalogoDAO catDao(db);
    AtendimentoDAO atendDao(db);

    if (profDao.listarTodos().empty()) {
        Profissional adminPadrao(0, "Thiago Fernandes", "ADMIN", "Administrador");
        profDao.inserir(adminPadrao);
    }

    // Autenticação
    std::string registroLogin;
    std::cout << "=== CICATRIZAR SYSTEM - LOGIN ===" << std::endl;
    std::cout << "Digite seu Registro (ou 'ADMIN' para primeiro acesso): ";
    std::getline(std::cin >> std::ws, registroLogin);

    Profissional usuarioLogado = profDao.buscarPorRegistro(registroLogin);
    if (usuarioLogado.getId() == 0) {
        std::cout << "Usuario nao encontrado. Encerrando o sistema." << std::endl;
        return 0;
    }

    int opcao = -1;
    while (opcao != 0) {
        std::cout << " CICATRIZAR SYSTEM | Usuario: " << usuarioLogado.getNome() 
                  << " (" << usuarioLogado.getNivelAcesso() << ")" << "\n" <<std::endl;
        std::cout << "1. Buscar paciente" << std::endl;
        std::cout << "2. Cadastrar paciente" << std::endl;
        std::cout << "3. Registrar atendimento (Procedimentos / Materiais)" << std::endl;
        std::cout << "4. Fechamento mensal" << std::endl;
        std::cout << "5. Cadastrar usuario (Admin)" << std::endl;
        std::cout << "0. Sair (Encerrar sessao)" << std::endl;
        std::cout << "Escolha uma acao: ";
        std::cin >> opcao;

        switch (opcao) {
            case 1: telaBuscarPaciente(pacDao); break;
            case 2: telaCadastrarPaciente(pacDao); break;
            case 3: telaRegistrarAtendimento(pacDao, catDao, atendDao, usuarioLogado); break;
            case 4: telaFechamentoMensal(atendDao); break;
            case 5: telaCadastrarUsuario(profDao, usuarioLogado); break;
            case 0: std::cout << "Encerrando sessao..." << std::endl; break;
            default: std::cout << "Opcao invalida!" << std::endl; break;
        }
    }

    return 0;
}