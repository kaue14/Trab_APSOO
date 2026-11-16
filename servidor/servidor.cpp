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
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <regex>
#include <string>

#include "../Database.h"
#include "../PacienteDAO.h"
#include "../ProfissionalDAO.h"
#include "../CatalogoDAO.h"
#include "../AtendimentoDAO.h"
#include "../LogAtividadeDAO.h"
#include "../ConfiguracaoDAO.h"
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

json paraJson(const DadosGerais& d) {
    return {{"nomeEmpresa", d.getNomeEmpresa()}, {"cnpj", d.getCnpj()}, {"telefone", d.getTelefone()},
            {"email", d.getEmail()}, {"cidade", d.getCidade()}, {"uf", d.getUf()}};
}

json paraJson(const LogAtividade& l) {
    return {{"id", l.getId()}, {"dataHora", l.getDataHora()}, {"profissionalId", l.getProfissionalId()},
            {"profissional", l.getNomeProfissional()}, {"tipo", l.getTipo()}, {"acao", l.getAcao()},
            {"detalhe", l.getDetalhe()}};
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

// ---------- Textos do log de atividades (RF013) ----------

// "2026-11-14" → "14/11/2026"
std::string dataBr(const std::string& iso) {
    if (iso.size() != 10) return iso;
    return iso.substr(8, 2) + "/" + iso.substr(5, 2) + "/" + iso.substr(0, 4);
}

// Junta partes como numa frase: "a", "a e b", "a, b e c"
std::string juntar(const std::vector<std::string>& partes) {
    std::string texto;
    for (size_t i = 0; i < partes.size(); i++) {
        if (i > 0) texto += (i == partes.size() - 1) ? " e " : ", ";
        texto += partes[i];
    }
    return texto;
}

std::string quantos(int n, const std::string& singular, const std::string& plural) {
    return std::to_string(n) + " " + (n == 1 ? singular : plural);
}

std::string nomeDoNivel(const std::string& nivel) {
    if (nivel == "Tecnico") return "Técnico de Enfermagem";
    return nivel;
}

// Edição de paciente: quais dados mudaram (RF013, "alterações de dados")
std::string alteracoesPaciente(const Paciente& antes, const Paciente& depois) {
    std::vector<std::string> campos;
    if (antes.getNome() != depois.getNome()) campos.push_back("nome");
    if (antes.getDataNascimento() != depois.getDataNascimento()) campos.push_back("data de nascimento");
    if (antes.getCpf() != depois.getCpf() || antes.getCarteira() != depois.getCarteira()) campos.push_back("documento");
    if (antes.getRua() != depois.getRua() || antes.getComplemento() != depois.getComplemento() ||
        antes.getBairro() != depois.getBairro() || antes.getCidade() != depois.getCidade() || antes.getUf() != depois.getUf()) {
        campos.push_back("endereço");
    }
    return campos.empty() ? "salvou sem alterações" : "alterou " + juntar(campos);
}

// ---------- Configurações (RF014) ----------

// 75.5 → "R$ 75,50"
std::string reais(double valor) {
    char texto[32];
    std::snprintf(texto, sizeof texto, "R$ %.2f", valor);
    std::string resultado = texto;
    resultado[resultado.find('.')] = ',';
    return resultado;
}

std::string formatarCnpj(const std::string& d) {
    return d.substr(0, 2) + "." + d.substr(2, 3) + "." + d.substr(5, 3) + "/" + d.substr(8, 4) + "-" + d.substr(12, 2);
}

std::string formatarTelefone(const std::string& d) {
    const size_t meio = d.size() - 4;   // (67) 3325-4180 ou (67) 99812-4180
    return "(" + d.substr(0, 2) + ") " + d.substr(2, meio - 2) + "-" + d.substr(meio);
}

// Lê e valida os dados gerais; devolve a mensagem de erro, ou "" se estiver certo (5.1)
std::string lerDadosGerais(const json& corpo, DadosGerais& d) {
    const std::string cnpj = apenasDigitos(textoDoCorpo(corpo, "cnpj"));
    const std::string telefone = apenasDigitos(textoDoCorpo(corpo, "telefone"));
    const std::string email = textoDoCorpo(corpo, "email");
    std::string uf = textoDoCorpo(corpo, "uf");
    for (char& c : uf) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    d.setNomeEmpresa(textoDoCorpo(corpo, "nomeEmpresa"));
    d.setCnpj(cnpj.size() == 14 ? formatarCnpj(cnpj) : "");
    d.setTelefone(telefone.size() == 10 || telefone.size() == 11 ? formatarTelefone(telefone) : "");
    d.setEmail(email);
    d.setCidade(textoDoCorpo(corpo, "cidade"));
    d.setUf(uf);

    if (d.getNomeEmpresa().empty()) return "Informe o nome da empresa.";
    if (cnpj.size() != 14) return "Informe o CNPJ com 14 dígitos.";
    if (!telefone.empty() && d.getTelefone().empty()) return "O telefone deve ter DDD e 8 ou 9 dígitos.";
    if (!email.empty() && !std::regex_match(email, std::regex(R"([^@\s]+@[^@\s]+\.[^@\s]+)"))) return "Informe um e-mail válido.";
    if (d.getCidade().empty() || !std::regex_match(uf, std::regex("[A-Z]{2}"))) return "Informe a cidade e a UF.";
    return "";
}

std::string alteracoesDadosGerais(const DadosGerais& antes, const DadosGerais& depois) {
    std::vector<std::string> campos;
    if (antes.getNomeEmpresa() != depois.getNomeEmpresa()) campos.push_back("nome da empresa");
    if (antes.getCnpj() != depois.getCnpj()) campos.push_back("CNPJ");
    if (antes.getTelefone() != depois.getTelefone()) campos.push_back("telefone");
    if (antes.getEmail() != depois.getEmail()) campos.push_back("e-mail");
    if (antes.getCidade() != depois.getCidade() || antes.getUf() != depois.getUf()) campos.push_back("cidade");
    return campos.empty() ? "salvou sem alterações" : "alterou " + juntar(campos);
}

// Descrição e valor de um item da tabela; devolve a mensagem de erro, ou "" se estiver certo
std::string lerItemCatalogo(const json& corpo, std::string& descricao, double& valor) {
    descricao = textoDoCorpo(corpo, "descricao");
    if (descricao.empty()) return "Informe a descrição do item.";
    if (!corpo.contains("valor") || !corpo["valor"].is_number()) return "Informe o valor em reais.";
    valor = std::round(corpo["valor"].get<double>() * 100) / 100;   // centavos
    if (valor <= 0 || valor >= 100000) return "O valor deve ser maior que zero e menor que R$ 100.000,00.";
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
    LogAtividadeDAO logDao(db);
    ConfiguracaoDAO configuracaoDao(db);

    // Texto do log para um atendimento: "Benedita Alves da Silva — atendimento de 14/11/2026: 2 procedimentos e 3 materiais"
    auto resumoAtendimento = [&](const Atendimento& a) {
        int procedimentos = 0, materiais = 0;
        for (const auto& item : a.getItens()) {
            if (catalogoDao.buscarPorId(item.getCatalogoId()).getTipo() == "Material") materiais++;
            else procedimentos++;
        }
        std::vector<std::string> partes;
        if (procedimentos > 0) partes.push_back(quantos(procedimentos, "procedimento", "procedimentos"));
        if (materiais > 0) partes.push_back(quantos(materiais, "material", "materiais"));
        return pacienteDao.buscarPorId(a.getPacienteId()).getNome() + " — atendimento de " +
               dataBr(a.getDataAtendimento()) + ": " + juntar(partes);
    };

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
        logDao.registrar(usuario.getId(), "Acesso", "Entrou no sistema", "Perfil " + nomeDoNivel(usuario.getNivelAcesso()));
        responder(res, {{"sessao", codigo}, {"usuario", paraJson(usuario)}});
    });

    servidor.Post("/api/logout", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        if (usuario.getId() != 0) logDao.registrar(usuario.getId(), "Acesso", "Saiu do sistema", "");
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
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirEnfermeiro(usuario, res)) return;

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
        logDao.registrar(usuario.getId(), "Paciente", "Cadastrou paciente", novo.getNome());
        responder(res, paraJson(novo), 201);
    });

    // RF008 — Editar dados do paciente (Enfermeiro e Administrador)
    servidor.Put(R"(/api/pacientes/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        const int id = std::stoi(req.matches[1]);
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirEnfermeiro(usuario, res)) return;
        const Paciente antes = pacienteDao.buscarPorId(id);
        if (antes.getId() == 0) return responderErro(res, 404, "Paciente não encontrado.");   // 1.2

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
        logDao.registrar(usuario.getId(), "Paciente", "Editou paciente",
                         alterado.getNome() + " — " + alteracoesPaciente(antes, alterado));
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
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirAdministrador(usuario, res)) return;   // 1.1

        const std::string nome = textoDoCorpo(corpo, "nome");
        const std::string registro = textoDoCorpo(corpo, "registroConselho");
        const std::string nivel = textoDoCorpo(corpo, "nivelAcesso");
        const std::string login = textoDoCorpo(corpo, "login");
        const std::string cpf = apenasDigitos(textoDoCorpo(corpo, "cpf"));
        const std::string senha = corpo.value("senha", std::string());

        // 2.1 — campos obrigatórios e formatos
        if (nome.empty() || registro.empty() || login.empty()) return responderErro(res, 400, "Informe o nome, o registro profissional (COREN) e o login.");
        if (nivel != "Tecnico" && nivel != "Enfermeiro" && nivel != "Administrador") return responderErro(res, 400, "Escolha o nível de acesso: Tecnico, Enfermeiro ou Administrador.");
        if (cpf.size() != 11) return responderErro(res, 400, "Informe o CPF com 11 dígitos.");   // UC004/UC005, passo 2
        // Começa com letra, para não ser confundido com um CPF na hora de entrar
        if (!std::regex_match(login, std::regex(R"([a-z][a-z0-9._-]{2,29})"))) {
            return responderErro(res, 400, "O login deve ter de 3 a 30 caracteres, começando por letra: letras minúsculas, números, ponto, hífen ou sublinhado.");
        }
        if (senha.size() < 6) return responderErro(res, 400, "A senha deve ter pelo menos 6 caracteres.");

        // 3.1 — login, CPF ou registro já usados
        if (profissionalDao.buscarPorLoginOuCpf(login, "").getId() != 0) return responderErro(res, 409, "Este login já está cadastrado. Escolha outro login.");
        if (profissionalDao.buscarPorLoginOuCpf("", cpf).getId() != 0) return responderErro(res, 409, "Já existe um usuário com este CPF.");
        if (profissionalDao.buscarPorRegistro(registro).getId() != 0) return responderErro(res, 409, "Já existe um usuário com este registro profissional.");

        Profissional novo(0, nome, registro, nivel);
        novo.setLogin(login);
        novo.setCpf(formatarCpf(cpf));
        novo.setSenhaHash(Seguranca::sha256(senha));
        if (!profissionalDao.inserir(novo)) return responderErro(res, 500, "Não foi possível cadastrar o usuário.");
        logDao.registrar(usuario.getId(), "Usuario", "Cadastrou usuário", nome + " (" + nomeDoNivel(nivel) + ")");
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
        const Atendimento registrado = atendimentoDao.buscarPorId(atendimento.getId());
        logDao.registrar(usuario.getId(), "Atendimento", "Registrou atendimento", resumoAtendimento(registrado));
        responder(res, paraJson(registrado), 201);
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
        const Atendimento salvo = atendimentoDao.buscarPorId(id);
        logDao.registrar(usuario.getId(), "Atendimento", "Editou atendimento", resumoAtendimento(salvo));
        responder(res, paraJson(salvo));
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

    // ---------- RF014 — Configurar sistema (alterações só pelo Administrador) ----------
    servidor.Get("/api/configuracoes", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirLogin(usuarioDaSessao(req), res)) return;
        responder(res, paraJson(configuracaoDao.carregarDadosGerais()));
    });

    servidor.Put("/api/configuracoes", [&](const httplib::Request& req, httplib::Response& res) {
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirAdministrador(usuario, res)) return;   // 1.1

        DadosGerais novos;
        const std::string erro = lerDadosGerais(corpo, novos);
        if (!erro.empty()) return responderErro(res, 400, erro);   // 5.1

        const DadosGerais antes = configuracaoDao.carregarDadosGerais();
        if (!configuracaoDao.salvarDadosGerais(novos)) return responderErro(res, 500, "Não foi possível salvar as configurações.");
        logDao.registrar(usuario.getId(), "Configuracao", "Alterou dados gerais", alteracoesDadosGerais(antes, novos));
        responder(res, paraJson(novos));
    });

    // Novo item na tabela de procedimentos ou na lista de materiais
    servidor.Post("/api/catalogo", [&](const httplib::Request& req, httplib::Response& res) {
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirAdministrador(usuario, res)) return;

        const std::string codigo = textoDoCorpo(corpo, "codigo");
        const std::string tipo = textoDoCorpo(corpo, "tipo");
        std::string descricao;
        double valor = 0;
        if (!std::regex_match(codigo, std::regex(R"([A-Za-z0-9/_.-]{3,20})"))) {
            return responderErro(res, 400, "Informe o código do item: de 3 a 20 letras ou números.");
        }
        if (tipo != "Procedimento" && tipo != "Material") return responderErro(res, 400, "Escolha o tipo: Procedimento ou Material.");
        const std::string erro = lerItemCatalogo(corpo, descricao, valor);
        if (!erro.empty()) return responderErro(res, 400, erro);
        if (catalogoDao.buscarPorCodigo(codigo).getId() != 0) return responderErro(res, 409, "Já existe um item com este código na tabela.");

        CatalogoItem novo(0, codigo, descricao, valor, tipo);
        if (!catalogoDao.inserir(novo)) return responderErro(res, 500, "Não foi possível cadastrar o item.");
        logDao.registrar(usuario.getId(), "Configuracao", tipo == "Material" ? "Cadastrou material" : "Cadastrou procedimento",
                         codigo + " — " + descricao + " (" + reais(valor) + ")");
        responder(res, paraJson(novo), 201);
    });

    // Altera a descrição ou o valor de um item (o código e o tipo não mudam)
    servidor.Put(R"(/api/catalogo/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        const int id = std::stoi(req.matches[1]);
        const json corpo = json::parse(req.body);
        std::lock_guard<std::mutex> trava(bancoEmUso);
        const Profissional usuario = usuarioDaSessao(req);
        if (!exigirAdministrador(usuario, res)) return;

        const CatalogoItem antes = catalogoDao.buscarPorId(id);
        if (antes.getId() == 0) return responderErro(res, 404, "Item não encontrado na tabela.");
        std::string descricao;
        double valor = 0;
        const std::string erro = lerItemCatalogo(corpo, descricao, valor);
        if (!erro.empty()) return responderErro(res, 400, erro);   // 5.1

        CatalogoItem alterado = antes;
        alterado.setDescricao(descricao);
        alterado.setValor(valor);
        if (!catalogoDao.atualizar(alterado)) return responderErro(res, 500, "Não foi possível salvar o item.");

        std::vector<std::string> mudancas;
        if (antes.getDescricao() != descricao) mudancas.push_back("descrição");
        if (reais(antes.getValor()) != reais(valor)) mudancas.push_back("valor de " + reais(antes.getValor()) + " para " + reais(valor));
        logDao.registrar(usuario.getId(), "Configuracao", antes.getTipo() == "Material" ? "Alterou material" : "Alterou procedimento",
                         antes.getCodigo() + " — " + (mudancas.empty() ? "salvou sem alterações" : "alterou " + juntar(mudancas)));
        responder(res, paraJson(alterado));
    });

    // ---------- RF013 — Log de atividades (só Administrador) ----------
    // As 500 ações mais recentes; a tela filtra por tipo, usuário e período
    servidor.Get("/api/log", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> trava(bancoEmUso);
        if (!exigirAdministrador(usuarioDaSessao(req), res)) return;   // 1.1
        responder(res, listaParaJson(logDao.listarRecentes()));         // 2.1 — lista vazia: a tela avisa
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
