// Camada de dados das telas.
// Aberto pelo sistema (cicatrizar.exe), conversa com a API e o banco de verdade (MODO_API = true).
// Aberto direto do arquivo, usa os dados fictícios do protótipo (pacientes.js, catalogo.js, atendimentos.js).
// Nos dois casos as telas chamam as mesmas funções, sempre com await.
//
// Formato que as telas usam:
//   usuario      { id, nome, perfil: 'tecnico' | 'enfermeiro' | 'administrador' }
//   paciente     { id, nome, nascimento, tipoDoc: 'CPF' | 'Carteira', documento, rua, complemento, bairro, cidade, uf }
//   item         { codigo, descricao, valor, tipo: 'procedimento' | 'material' }
//   profissional { id, nome, coren }
//   atendimento  { id, pacienteId, data, profissionalId, registradoPor, itens: [{ codigo, quantidade }] }

// Erro com a mensagem pronta para mostrar na tela (as do servidor seguem os casos de uso)
class ErroDoSistema extends Error {
  constructor(mensagem, status = 0) {
    super(mensagem);
    this.status = status;
  }
}

const DadosApi = (() => {
  function lerSessao() {
    try {
      return sessionStorage.getItem('sessao') || '';
    } catch {
      return '';
    }
  }

  function guardarSessao(codigo) {
    try {
      if (codigo) sessionStorage.setItem('sessao', codigo);
      else sessionStorage.removeItem('sessao');
    } catch {}
  }

  async function chamar(metodo, caminho, corpo) {
    let resposta;
    try {
      resposta = await fetch(caminho, {
        method: metodo,
        headers: { 'Content-Type': 'application/json', 'X-Sessao': lerSessao() },
        body: corpo === undefined ? undefined : JSON.stringify(corpo),
      });
    } catch {
      throw new ErroDoSistema('Não foi possível falar com o sistema. Confira se o programa está aberto e tente de novo.');
    }

    const dados = await resposta.json().catch(() => ({}));
    // Sessão vencida (o programa foi fechado e aberto de novo, por exemplo): volta para o login
    if (resposta.status === 401 && !location.pathname.endsWith('login.html')) {
      guardarSessao('');
      location.href = 'login.html?sessao=expirada';
      return new Promise(() => {});   // a página está saindo; nada mais acontece aqui
    }
    if (!resposta.ok) throw new ErroDoSistema(dados.erro || 'Não foi possível concluir a operação.', resposta.status);
    return dados;
  }

  const PERFIL_DO_NIVEL = { Tecnico: 'tecnico', Enfermeiro: 'enfermeiro', Administrador: 'administrador' };
  const usuarioDe = (p) => ({ id: p.id, nome: p.nome, perfil: PERFIL_DO_NIVEL[p.nivelAcesso] || 'tecnico' });

  // O banco guarda CPF e carteira separados; a tela mostra um dos dois (a carteira, se houver)
  const pacienteDe = (p) => ({
    id: p.id, nome: p.nome, nascimento: p.dataNascimento,
    tipoDoc: p.carteira ? 'Carteira' : 'CPF', documento: p.carteira || p.cpf,
    rua: p.rua, complemento: p.complemento, bairro: p.bairro, cidade: p.cidade, uf: p.uf || 'MS',
  });

  // Ao salvar, o documento que a tela não mostrou continua como estava
  const pacienteParaApi = (dados, atual = {}) => ({
    nome: dados.nome, dataNascimento: dados.nascimento,
    cpf: dados.tipoDoc === 'CPF' ? dados.documento : (atual.cpf || ''),
    carteira: dados.tipoDoc === 'Carteira' ? dados.documento : (atual.carteira || ''),
    rua: dados.rua, complemento: dados.complemento, bairro: dados.bairro, cidade: dados.cidade, uf: dados.uf,
  });

  // Tabela de procedimentos e materiais: lida uma vez; a tela usa o código, a API usa o id
  let catalogo = null;
  async function catalogoDaApi() {
    if (!catalogo) catalogo = await chamar('GET', '/api/catalogo');
    return catalogo;
  }

  const atendimentoDe = (a) => ({
    id: a.id, pacienteId: a.pacienteId, data: a.dataAtendimento, profissionalId: a.profissionalId,
    registradoPor: a.registradoPor, itens: a.itens.map((item) => ({ codigo: item.codigo, quantidade: item.quantidade })),
  });

  async function atendimentoParaApi(dados) {
    const tabela = await catalogoDaApi();
    return {
      pacienteId: dados.pacienteId, profissionalId: dados.profissionalId, dataAtendimento: dados.data,
      itens: dados.itens.map((item) => ({
        catalogoId: tabela.find((linha) => linha.codigo === item.codigo).id, quantidade: item.quantidade,
      })),
    };
  }

  return {
    async entrar(login, senha) {
      const resposta = await chamar('POST', '/api/login', { login, senha });
      guardarSessao(resposta.sessao);
      return usuarioDe(resposta.usuario);
    },
    async sair() {
      await chamar('POST', '/api/logout').catch(() => {});
      guardarSessao('');
    },
    async usuarioLogado() {
      return usuarioDe(await chamar('GET', '/api/sessao'));
    },

    async buscarPacientes(termo) {
      return (await chamar('GET', `/api/pacientes?busca=${encodeURIComponent(termo)}`)).map(pacienteDe);
    },
    async obterPaciente(id) {
      try {
        return pacienteDe(await chamar('GET', `/api/pacientes/${Number(id)}`));
      } catch (erro) {
        if (erro.status === 404) return null;
        throw erro;
      }
    },
    async cadastrarPaciente(dados) {
      return pacienteDe(await chamar('POST', '/api/pacientes', pacienteParaApi(dados)));
    },
    async atualizarPaciente(id, dados) {
      const atual = await chamar('GET', `/api/pacientes/${id}`);
      return pacienteDe(await chamar('PUT', `/api/pacientes/${id}`, pacienteParaApi(dados, atual)));
    },

    async listarCatalogo() {
      return (await catalogoDaApi()).map((item) => ({
        codigo: item.codigo, descricao: item.descricao, valor: item.valor, tipo: item.tipo.toLowerCase(),
      }));
    },
    async listarProfissionais() {
      return (await chamar('GET', '/api/profissionais')).map((p) => ({ id: p.id, nome: p.nome, coren: p.registroConselho }));
    },

    async listarAtendimentos(pacienteId) {
      return (await chamar('GET', `/api/atendimentos?paciente=${pacienteId}`)).map(atendimentoDe);
    },
    async registrarAtendimento(dados) {
      return atendimentoDe(await chamar('POST', '/api/atendimentos', await atendimentoParaApi(dados)));
    },
    async atualizarAtendimento(id, dados) {
      return atendimentoDe(await chamar('PUT', `/api/atendimentos/${id}`, await atendimentoParaApi(dados)));
    },
  };
})();

const DadosFicticios = (() => {
  // No protótipo a pessoa é sempre a mesma; o perfil vem do painel "ver como"
  const USUARIO = { id: 1, nome: 'Thiago Fernandes' };

  const proximoId = (lista) => Math.max(0, ...lista.map((registro) => registro.id)) + 1;
  const maisRecentePrimeiro = (a, b) => b.data.localeCompare(a.data) || b.id - a.id;

  return {
    async sair() {
      try {
        sessionStorage.removeItem('perfil');
      } catch {}
    },
    async usuarioLogado() {
      let perfil = 'tecnico';
      try {
        perfil = sessionStorage.getItem('perfil') || perfil;
      } catch {}
      return { ...USUARIO, perfil };
    },

    async buscarPacientes(termo) {
      const nome = normalizar(termo);
      const numero = soDigitos(termo);
      return carregarPacientes()
        .filter((paciente) => normalizar(paciente.nome).includes(nome) ||
          (numero.length >= 3 && soDigitos(paciente.documento).includes(numero)))
        .sort((a, b) => a.nome.localeCompare(b.nome, 'pt-BR'));
    },
    async obterPaciente(id) {
      return carregarPacientes().find((paciente) => paciente.id === Number(id)) || null;
    },
    async cadastrarPaciente(dados) {
      const pacientes = carregarPacientes();
      const novo = { id: proximoId(pacientes), ...dados };
      pacientes.push(novo);
      salvarPacientes(pacientes);
      return novo;
    },
    async atualizarPaciente(id, dados) {
      const pacientes = carregarPacientes();
      const paciente = pacientes.find((p) => p.id === Number(id));
      if (!paciente) throw new ErroDoSistema('Paciente não encontrado.', 404);
      Object.assign(paciente, dados);
      salvarPacientes(pacientes);
      return paciente;
    },

    async listarCatalogo() {
      return CATALOGO;
    },
    async listarProfissionais() {
      return PROFISSIONAIS;
    },

    async listarAtendimentos(pacienteId) {
      return carregarAtendimentos().filter((atendimento) => atendimento.pacienteId === pacienteId).sort(maisRecentePrimeiro);
    },
    async registrarAtendimento(dados) {
      const atendimentos = carregarAtendimentos();
      const novo = { id: proximoId(atendimentos), registradoPor: USUARIO.id, ...dados };
      atendimentos.push(novo);
      salvarAtendimentos(atendimentos);
      return novo;
    },
    async atualizarAtendimento(id, dados) {
      const atendimentos = carregarAtendimentos();
      const atendimento = atendimentos.find((a) => a.id === id);
      if (!atendimento) throw new ErroDoSistema('Atendimento não encontrado.', 404);
      Object.assign(atendimento, dados);
      salvarAtendimentos(atendimentos);
      return atendimento;
    },
  };
})();

const Dados = MODO_API ? DadosApi : DadosFicticios;
