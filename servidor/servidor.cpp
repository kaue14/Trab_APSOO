// Servidor web do Cicatrizar System.
// Abre as telas da pasta prototipo/ e responde às telas com os dados do banco, usando os DAOs.
// É a camada de controle entre as telas (HTML) e as classes de acesso a dados (DAO).
//
// Compilar (na pasta do projeto):
//   g++ -std=c++17 -static servidor/servidor.cpp sqlite3.o -o cicatrizar -lws2_32 -lbcrypt
// Rodar (na pasta do projeto):
//   cicatrizar                      abre o sistema numa janela do Edge
//   cicatrizar --sem-janela         só inicia o servidor (http://localhost:8080)
//   cicatrizar outro-banco.db       usa outro arquivo de banco

#include "lib/httplib.h"   // incluir antes de windows.h (o httplib usa winsock2.h)
#include "lib/json.hpp"

#include <windows.h>
#include <shellapi.h>

#include <cctype>
#include <map>
#include <mutex>
#include <regex>
#include <string>

#include "../Database.h"
#include "../PacienteDAO.h"
#include "../ProfissionalDAO.h"
#include "../CatalogoDAO.h"
#include "../AtendimentoDAO.h"
#include "../Seguranca.h"

using json = nlohmann::json;

// Usuário criado no primeiro acesso, quando ninguém tem senha ainda (troque depois de entrar)
const std::string LOGIN_INICIAL = "admin";
const std::string SENHA_INICIAL = "cicatrizar";

// ---------- Conversão das classes para JSON (o formato que as telas recebem) ----------

json paraJson(const Paciente& p) {
    return {{"id", p.getId()}, {"nome", p.getNome()}, {"dataNascimento", p.getDataNascimento()},
            {"cpf", p.getCpf()}, {"carteira", p.getCarteira()},
            {"rua", p.getRua()}, {"complemento", p.getComplemento()}, {"bairro", p.getBairro()},
            {"cidade", p.getCidade()}, {"uf", p.getUf()}};
}

// O hash da senha nunca sai do servidor
json paraJson(const Profissional& p) {
    return {{"id", p.getId()}, {"nome", p.getNome()}, {"registroConselho", p.getRegistroConselho()},
            {"nivelAcesso", p.getNivelAcesso()}, {"login", p.getLogin()}, {"cpf", p.getCpf()}};
}

json paraJson(const CatalogoItem& i) {
    return {{"id", i.getId()}, {"codigo", i.getCodigo()}, {"descricao", i.getDescricao()},
            {"valor", i.getValor()}, {"tipo", i.getTipo()}};
}

json paraJson(const Atendimento& a) {
    json itens = json::array();
    for (const auto& item : a.getItens()) {
        itens.push_back({{"id", item.getId()}, {"catalogoId", item.getCatalogoId()},
                         {"codigo", item.getCodigoItem()}, {"descricao", item.getDescricaoItem()},
                         {"quantidade", item.getQuantidade()}, {"valorUnitario", item.getValorUnitario()},
                         {"subtotal", item.getSubtotal()}});
    }
    return {{"id", a.getId()}, {"pacienteId", a.getPacienteId()}, {"profissionalId", a.getProfissionalId()},
            {"dataAtendimento", a.getDataAtendimento()}, {"registradoPor", a.getRegistradoPor()},
            {"itens", itens}, {"total", a.calcularTotalGuia()}};
}

template <typename T>
json listaParaJson(const std::vector<T>& lista) {
    json resultado = json::array();
    for (const auto& elemento : lista) resultado.push_back(paraJson(elemento));
    return resultado;
}

// ---------- Respostas e verificações ----------

void responder(httplib::Response& res, const json& corpo, int status = 200) {
    res.status = status;
    res.set_content(corpo.dump(), "application/json; charset=utf-8");
}

void responderErro(httplib::Response& res, int status, const std::string& mensagem) {
    responder(res, {{"erro", mensagem}}, status);
}

// NF003 — cada rota confere se há sessão e se o perfil pode usar a função
bool exigirLogin(const Profissional& usuario, httplib::Response& res) {
    if (usuario.getId() != 0) return true;
    responderErro(res, 401, "Sessão encerrada ou inválida. Entre novamente.");
    return false;
}

bool exigirEnfermeiro(const Profissional& usuario, httplib::Response& res) {
    if (!exigirLogin(usuario, res)) return false;
    if (usuario.isEnfermeiroOuSuperior()) return true;
    responderErro(res, 403, "Acesso não permitido para o seu perfil.");
    return false;
}

bool exigirAdministrador(const Profissional& usuario, httplib::Response& res) {
    if (!exigirLogin(usuario, res)) return false;
    if (usuario.isAdministrador()) return true;
    responderErro(res, 403, "Apenas Administradores podem usar esta função.");
    return false;
}

std::string aparar(const std::string& texto) {
    const auto inicio = texto.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) return "";
    return texto.substr(inicio, texto.find_last_not_of(" \t\r\n") - inicio + 1);
}

std::string apenasDigitos(const std::string& texto) {
    std::string digitos;
    for (char c : texto) {
        if (std::isdigit(static_cast<unsigned char>(c))) digitos += c;
    }
    return digitos;
}

// Guarda sempre no mesmo formato, digitado com ou sem pontuação
std::string formatarCpf(const std::string& d) {
    return d.substr(0, 3) + "." + d.substr(3, 3) + "." + d.substr(6, 3) + "-" + d.substr(9, 2);
}

std::string formatarCarteira(const std::string& d) {
    return d.substr(0, 4) + " " + d.substr(4, 4) + " " + d.substr(8, 4) + " " + d.substr(12, 4);
}

bool dataValida(const std::string& data) {
    return std::regex_match(data, std::regex(R"((19|20)\d{2}-(0[1-9]|1[0-2])-(0[1-9]|[12]\d|3[01]))"));
}

bool mesValido(const std::string& mes) {
    return std::regex_match(mes, std::regex(R"((19|20)\d{2}-(0[1-9]|1[0-2]))"));
}

std::string textoDoCorpo(const json& corpo, const char* campo) {
    return aparar(corpo.value(campo, std::string()));
}

// RF006/RF008 — lê e valida os dados do paciente; devolve a mensagem de erro, ou "" se estiver certo
std::string lerPaciente(const json& corpo, Paciente& p) {
    const std::string cpf = apenasDigitos(textoDoCorpo(corpo, "cpf"));
    const std::string carteira = apenasDigitos(textoDoCorpo(corpo, "carteira"));
    const std::string uf = textoDoCorpo(corpo, "uf");

    p.setNome(textoDoCorpo(corpo, "nome"));
    p.setDataNascimento(textoDoCorpo(corpo, "dataNascimento"));
    p.setCpf(cpf.size() == 11 ? formatarCpf(cpf) : "");
    p.setCarteira(carteira.size() == 16 ? formatarCarteira(carteira) : "");
    p.setEndereco(textoDoCorpo(corpo, "rua"), textoDoCorpo(corpo, "complemento"), textoDoCorpo(corpo, "bairro"),
                  textoDoCorpo(corpo, "cidade"), uf);

    if (p.getNome().empty()) return "Informe o nome completo.";
    if (!dataValida(p.getDataNascimento())) return "Informe a data de nascimento no formato AAAA-MM-DD.";
    if (cpf.empty() && carteira.empty()) return "Informe o CPF ou o número da carteira.";
    if (!cpf.empty() && cpf.size() != 11) return "O CPF deve ter 11 dígitos.";
    if (!carteira.empty() && carteira.size() != 16) return "A carteira deve ter 16 dígitos.";
    if (p.getRua().empty() || p.getBairro().empty() || p.getCidade().empty() || uf.size() != 2) {
        return "Informe o endereço completo: logradouro, bairro, cidade e UF.";
    }
    return "";
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    std::string caminhoBanco = "homecare.db";
    bool abrirJanela = true;
    for (int i = 1; i < argc; i++) {
        std::string argumento = argv[i];
        if (argumento == "--sem-janela") abrirJanela = false;
        else caminhoBanco = argumento;
    }

    Database db(caminhoBanco);
    PacienteDAO pacienteDao(db);
    ProfissionalDAO profissionalDao(db);
    CatalogoDAO catalogoDao(db);
    AtendimentoDAO atendimentoDao(db);

    // Primeiro acesso: se ninguém tem senha, cria (ou completa) o administrador inicial
    bool alguemComSenha = false;
    for (const auto& p : profissionalDao.listarTodos()) alguemComSenha = alguemComSenha || !p.getSenhaHash().empty();
    if (!alguemComSenha) {
        Profissional admin = profissionalDao.buscarPorLoginOuCpf(LOGIN_INICIAL, "");
        if (admin.getId() == 0) admin = profissionalDao.buscarPorRegistro("ADMIN");   // o do main.cpp
        if (admin.getId() == 0) {
            Profissional novo(0, "Administrador", "ADMIN", "Administrador");
            novo.setLogin(LOGIN_INICIAL);
            novo.setSenhaHash(Seguranca::sha256(SENHA_INICIAL));
            profissionalDao.inserir(novo);
        } else {
            profissionalDao.definirAcesso(admin.getId(), LOGIN_INICIAL, Seguranca::sha256(SENHA_INICIAL));
        }
        std::cout << "[Primeiro acesso] Entre com o login '" << LOGIN_INICIAL << "' (a senha inicial está no README)." << std::endl;
    }

    // O servidor atende vários pedidos ao mesmo tempo; o banco e as sessões são usados por um de cada vez
    std::mutex bancoEmUso;

    // Sessões abertas: código da sessão → id do profissional (somem ao fechar o programa)
    std::map<std::string, int> sessoes;

    auto usuarioDaSessao = [&](const httplib::Request& req) {
        const auto sessao = sessoes.find(req.get_header_value("X-Sessao"));
        if (sessao == sessoes.end()) return Profissional();
        return profissionalDao.buscarPorId(sessao->second);
    };

    httplib::Server servidor;

    if (!servidor.set_mount_point("/", "./prototipo")) {
        std::cerr << "[Erro] Pasta 'prototipo' não encontrada. Rode o programa na pasta do projeto." << std::endl;
        return 1;
    }
    servidor.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/login.html");
    });

    // As telas perguntam por js/modo.js: aberto pelo servidor, passam a usar a API em vez dos dados fictícios
    servidor.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.path != "/js/modo.js") return httplib::Server::HandlerResponse::Unhandled;
        res.set_header("Cache-Control", "no-cache");
        res.set_content("const MODO_API = true;\n", "text/javascript; charset=utf-8");
        return httplib::Server::HandlerResponse::Handled;
    });

    // O navegador sempre confere se as telas mudaram (sem isso, mostra versões antigas guardadas)
    servidor.set_file_request_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-cache");
    });

    // JSON malformado ou com tipos errados vira erro 400 com mensagem, em vez de derrubar o pedido
    servidor.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr erro) {
        try {
            std::rethrow_exception(erro);
        } catch (const json::exception&) {
            responderErro(res, 400, "Dados enviados em formato inválido.");
        } catch (const std::logic_error&) {   // ex.: ?paciente=abc, que não é um número
            responderErro(res, 400, "Dados enviados em formato inválido.");
        } catch (const std::exception& e) {
            responderErro(res, 500, std::string("Erro interno: ") + e.what());
        }
    });

    // ---------- RF001 — Autenticar usuário / RF003 — Encerrar sessão ----------
    servidor.Post("/api/login", [&](const httplib::Request& req, httplib::Response& res) {
        const json corpo = json::parse(req.body);
        const std::string login = textoDoCorpo(corpo, "login");
        const std::string senha = corpo.value("senha", std::string());
        if (login.empty() || senha.empty()) return responderErro(res, 400, "Informe o login (ou CPF) e a senha.");   // 2.1

        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = profissionalDao.buscarPorLoginOuCpf(login, apenasDigitos(login));
        if (usuario.getId() == 0 || usuario.getSenhaHash().empty() || usuario.getSenhaHash() != Seguranca::sha256(senha)) {
            return responderErro(res, 401, "Login/CPF ou senha incorretos. Verifique os dados e tente novamente.");   // 1.1
        }

        const std::string codigo = Seguranca::gerarCodigoSessao();
        sessoes[codigo] = usuario.getId();
        responder(res, {{"sessao", codigo}, {"usuario", paraJson(usuario)}});
    });

    servidor.Post("/api/logout", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        sessoes.erase(req.get_header_value("X-Sessao"));   // invalida a sessão ativa
        responder(res, {{"ok", true}});
    });

    // Quem está logado (as telas usam ao abrir)
    servidor.Get("/api/sessao", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirLogin(usuario, res)) return;
        responder(res, paraJson(usuario));
    });

    // ---------- Pacientes ----------
    // RF007 — ?busca= procura por nome ou parte do CPF/carteira; sem parâmetro, lista todos
    servidor.Get("/api/pacientes", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirLogin(usuarioDaSessao(req), res)) return;
        if (req.has_param("busca")) {
            const std::string termo = aparar(req.get_param_value("busca"));
            if (termo.empty()) return responderErro(res, 400, "Informe um nome ou número de identificação para pesquisar.");
            return responder(res, listaParaJson(pacienteDao.buscarPorTermo(termo)));
        }
        responder(res, listaParaJson(pacienteDao.listarTodos()));
    });

    servidor.Get(R"(/api/pacientes/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirLogin(usuarioDaSessao(req), res)) return;
        const Paciente paciente = pacienteDao.buscarPorId(std::stoi(req.matches[1]));
        if (paciente.getId() == 0) return responderErro(res, 404, "Paciente não encontrado.");
        responder(res, paraJson(paciente));
    });

    // RF006 — Cadastrar paciente (Enfermeiro e Administrador)
    servidor.Post("/api/pacientes", [&](const httplib::Request& req, httplib::Response& res) {
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirEnfermeiro(usuarioDaSessao(req), res)) return;

        Paciente novo;
        const std::string erro = lerPaciente(corpo, novo);
        if (!erro.empty()) return responderErro(res, 400, erro);
        if (!novo.getCpf().empty() && pacienteDao.buscarPorCpf(novo.getCpf()).getId() != 0) {
            return responderErro(res, 409, "Já existe um paciente cadastrado com este CPF.");
        }
        if (!novo.getCarteira().empty() && pacienteDao.buscarPorCarteira(novo.getCarteira()).getId() != 0) {
            return responderErro(res, 409, "Já existe um paciente cadastrado com esta carteira.");
        }
        if (!pacienteDao.inserir(novo)) return responderErro(res, 500, "Não foi possível salvar o paciente.");
        responder(res, paraJson(novo), 201);
    });

    // RF008 — Editar dados do paciente (Enfermeiro e Administrador)
    servidor.Put(R"(/api/pacientes/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        const int id = std::stoi(req.matches[1]);
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirEnfermeiro(usuarioDaSessao(req), res)) return;
        if (pacienteDao.buscarPorId(id).getId() == 0) return responderErro(res, 404, "Paciente não encontrado.");   // 1.2

        Paciente alterado;
        const std::string erro = lerPaciente(corpo, alterado);
        if (!erro.empty()) return responderErro(res, 400, erro);   // 5.1
        alterado.setId(id);

        const Paciente mesmoCpf = pacienteDao.buscarPorCpf(alterado.getCpf());
        if (!alterado.getCpf().empty() && mesmoCpf.getId() != 0 && mesmoCpf.getId() != id) {
            return responderErro(res, 409, "Já existe outro paciente cadastrado com este CPF.");
        }
        const Paciente mesmaCarteira = pacienteDao.buscarPorCarteira(alterado.getCarteira());
        if (!alterado.getCarteira().empty() && mesmaCarteira.getId() != 0 && mesmaCarteira.getId() != id) {
            return responderErro(res, 409, "Já existe outro paciente cadastrado com esta carteira.");
        }
        if (!pacienteDao.atualizar(alterado)) return responderErro(res, 500, "Não foi possível salvar as alterações.");
        responder(res, paraJson(alterado));
    });

    // ---------- Profissionais ----------
    servidor.Get("/api/profissionais", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirLogin(usuarioDaSessao(req), res)) return;
        responder(res, listaParaJson(profissionalDao.listarTodos()));
    });

    // RF002/RF004/RF005 — Cadastrar usuário (só Administrador)
    servidor.Post("/api/profissionais", [&](const httplib::Request& req, httplib::Response& res) {
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirAdministrador(usuarioDaSessao(req), res)) return;   // 1.1

        const std::string nome = textoDoCorpo(corpo, "nome");
        const std::string registro = textoDoCorpo(corpo, "registroConselho");
        const std::string nivel = textoDoCorpo(corpo, "nivelAcesso");
        const std::string login = textoDoCorpo(corpo, "login");
        const std::string cpf = apenasDigitos(textoDoCorpo(corpo, "cpf"));
        const std::string senha = corpo.value("senha", std::string());

        // 2.1 — campos obrigatórios e formatos
        if (nome.empty() || registro.empty() || login.empty()) return responderErro(res, 400, "Informe o nome, o registro profissional (COREN) e o login.");
        if (nivel != "Tecnico" && nivel != "Enfermeiro" && nivel != "Administrador") return responderErro(res, 400, "Escolha o nível de acesso: Tecnico, Enfermeiro ou Administrador.");
        if (!cpf.empty() && cpf.size() != 11) return responderErro(res, 400, "O CPF deve ter 11 dígitos.");
        if (senha.size() < 6) return responderErro(res, 400, "A senha deve ter pelo menos 6 caracteres.");

        // 3.1 — login, CPF ou registro já usados
        if (profissionalDao.buscarPorLoginOuCpf(login, "").getId() != 0) return responderErro(res, 409, "Este login já está cadastrado. Escolha outro login.");
        if (!cpf.empty() && profissionalDao.buscarPorLoginOuCpf("", cpf).getId() != 0) return responderErro(res, 409, "Já existe um usuário com este CPF.");
        if (profissionalDao.buscarPorRegistro(registro).getId() != 0) return responderErro(res, 409, "Já existe um usuário com este registro profissional.");

        Profissional novo(0, nome, registro, nivel);
        novo.setLogin(login);
        novo.setCpf(cpf.empty() ? "" : formatarCpf(cpf));
        novo.setSenhaHash(Seguranca::sha256(senha));
        if (!profissionalDao.inserir(novo)) return responderErro(res, 500, "Não foi possível cadastrar o usuário.");
        responder(res, paraJson(novo), 201);
    });

    // ---------- Catálogo de procedimentos e materiais ----------
    // ?tipo=Procedimento | Material   ?busca=curativo
    servidor.Get("/api/catalogo", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirLogin(usuarioDaSessao(req), res)) return;
        if (req.has_param("busca")) return responder(res, listaParaJson(catalogoDao.buscarPorTermo(req.get_param_value("busca"))));
        if (req.has_param("tipo")) return responder(res, listaParaJson(catalogoDao.listarPorTipo(req.get_param_value("tipo"))));
        responder(res, listaParaJson(catalogoDao.listarTodos()));
    });

    // ---------- Atendimentos (RF009, RF010, RF012) ----------
    // Lê data, profissional e itens enviados pela tela; devolve a mensagem de erro, ou "" se estiver certo
    auto lerAtendimento = [&](const json& corpo, Atendimento& atendimento) -> std::string {
        const int profissionalId = corpo.value("profissionalId", 0);
        const std::string data = textoDoCorpo(corpo, "dataAtendimento");
        const json itens = corpo.value("itens", json::array());

        if (!dataValida(data)) return "Informe a data do atendimento no formato AAAA-MM-DD.";   // 3.1
        if (profissionalDao.buscarPorId(profissionalId).getId() == 0) return "Selecione o profissional executante.";
        if (!itens.is_array() || itens.empty()) return "Adicione pelo menos um procedimento ou material.";

        atendimento = Atendimento(atendimento.getId(), atendimento.getPacienteId(), profissionalId, data);
        for (const auto& item : itens) {
            const int catalogoId = item.value("catalogoId", 0);
            const int quantidade = item.value("quantidade", 0);
            if (catalogoDao.buscarPorId(catalogoId).getId() == 0) return "Item fora da tabela de procedimentos e materiais.";
            if (quantidade < 1) return "A quantidade deve ser maior que zero.";
            atendimento.adicionarItem(catalogoId, quantidade);
        }
        return "";
    };

    // ?paciente=3 → histórico do paciente (todos)   ?mes=2026-11 → todos do mês (Enfermeiro e Administrador)
    servidor.Get("/api/atendimentos", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        const std::string mes = req.get_param_value("mes");
        if (!mes.empty() && !mesValido(mes)) return responderErro(res, 400, "Informe o mês no formato AAAA-MM (ex.: 2026-11).");

        if (req.has_param("paciente")) {
            if (!exigirLogin(usuario, res)) return;
            return responder(res, listaParaJson(atendimentoDao.listarPorPaciente(std::stoi(req.get_param_value("paciente")), mes)));
        }
        if (!exigirEnfermeiro(usuario, res)) return;
        if (mes.empty()) return responderErro(res, 400, "Informe o paciente ou o mês.");
        responder(res, listaParaJson(atendimentoDao.listarPorMes(mes)));
    });

    servidor.Get(R"(/api/atendimentos/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirLogin(usuarioDaSessao(req), res)) return;
        const Atendimento atendimento = atendimentoDao.buscarPorId(std::stoi(req.matches[1]));
        if (atendimento.getId() == 0) return responderErro(res, 404, "Atendimento não encontrado.");
        responder(res, paraJson(atendimento));
    });

    // RF009/RF010 — Registrar procedimentos e materiais de um atendimento
    servidor.Post("/api/atendimentos", [&](const httplib::Request& req, httplib::Response& res) {
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirLogin(usuario, res)) return;

        const int pacienteId = corpo.value("pacienteId", 0);
        if (pacienteDao.buscarPorId(pacienteId).getId() == 0) return responderErro(res, 400, "Selecione um paciente cadastrado.");   // 1.1

        Atendimento atendimento(0, pacienteId, 0, "");
        const std::string erro = lerAtendimento(corpo, atendimento);
        if (!erro.empty()) return responderErro(res, 400, erro);
        atendimento.setRegistradoPor(usuario.getId());   // quem registrou é quem está logado

        if (!atendimentoDao.registrarAtendimento(atendimento)) return responderErro(res, 500, "Não foi possível registrar o atendimento.");   // 5.1
        responder(res, paraJson(atendimentoDao.buscarPorId(atendimento.getId())), 201);
    });

    // RF012 — Editar atendimento registrado
    servidor.Put(R"(/api/atendimentos/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        const int id = std::stoi(req.matches[1]);
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirLogin(usuario, res)) return;

        const Atendimento atual = atendimentoDao.buscarPorId(id);
        if (atual.getId() == 0) return responderErro(res, 404, "Atendimento não encontrado.");   // 1.1
        // 3.1 — Técnico só edita o que ele mesmo registrou
        if (!usuario.isEnfermeiroOuSuperior() && atual.getRegistradoPor() != usuario.getId()) {
            return responderErro(res, 403, "Você só pode editar atendimentos registrados por você.");
        }

        Atendimento corrigido(id, atual.getPacienteId(), 0, "");
        const std::string erro = lerAtendimento(corpo, corrigido);
        if (!erro.empty()) return responderErro(res, 400, erro);   // 5.1
        corrigido.setRegistradoPor(atual.getRegistradoPor());

        if (!atendimentoDao.atualizarAtendimento(corrigido)) return responderErro(res, 500, "Não foi possível salvar as alterações.");
        responder(res, paraJson(atendimentoDao.buscarPorId(id)));
    });

    // ---------- RF011 — Fechamento mensal de um paciente (Enfermeiro e Administrador) ----------
    servidor.Get("/api/fechamento", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirEnfermeiro(usuarioDaSessao(req), res)) return;   // 1.1

        const std::string mes = req.get_param_value("mes");
        const std::string paciente = req.get_param_value("paciente");
        if (paciente.empty() || !mesValido(mes)) return responderErro(res, 400, "Selecione o paciente e informe o mês e o ano.");   // 2.1
        const int pacienteId = std::stoi(paciente);
        if (pacienteDao.buscarPorId(pacienteId).getId() == 0) return responderErro(res, 404, "Paciente não encontrado.");

        // 2.2 — sem registros no período, a lista volta vazia e a tela avisa
        responder(res, {{"pacienteId", pacienteId}, {"mes", mes},
                        {"total", atendimentoDao.calcularFechamentoMensal(mes, pacienteId)},
                        {"atendimentos", listaParaJson(atendimentoDao.listarPorPaciente(pacienteId, mes))}});
    });

    // ---------- Inicia ----------
    const int porta = 8080;
    if (!servidor.bind_to_port("127.0.0.1", porta)) {
        std::cerr << "[Erro] A porta " << porta << " já está em uso. O sistema já está aberto?" << std::endl;
        return 1;
    }

    const std::string endereco = "http://localhost:" + std::to_string(porta) + "/login.html";
    std::cout << "\nCicatrizar System rodando em " << endereco << " (Ctrl+C para encerrar)" << std::endl;

    // NF005 — abre numa janela própria: Edge em "modo aplicativo", sem barra de endereço
    if (abrirJanela) {
        const std::string parametros = "--app=" + endereco;
        ShellExecuteA(nullptr, "open", "msedge", parametros.c_str(), nullptr, SW_SHOWNORMAL);
    }

    servidor.listen_after_bind();
    return 0;
}
