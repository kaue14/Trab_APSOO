// Camada de dados das telas.
// Aberto pelo sistema (cicatrizar.exe), conversa com a API e o banco de verdade (MODO_API = true).
// Aberto direto do arquivo, usa os dados fictícios do protótipo (pacientes.js, catalogo.js, atendimentos.js).
// Nos dois casos as telas chamam as mesmas funções, sempre com await.
//
// Formato que as telas usam:
//   usuario      { id, nome, perfil: 'tecnico' | 'enfermeiro' | 'administrador' }
//                na tela Usuários, também { coren, login, cpf }
//   paciente     { id, nome, nascimento, tipoDoc: 'CPF' | 'Carteira', documento, rua, complemento, bairro, cidade, uf }
//   item         { codigo, descricao, valor, tipo: 'procedimento' | 'material' }
//   profissional { id, nome, coren }
//   atendimento  { id, pacienteId, data, profissionalId, registradoPor, itens: [{ codigo, quantidade }] }
//   registro     { id, dataHora: 'aaaa-mm-dd hh:mm:ss', profissionalId, usuario (nome),
//                  tipo: 'acesso' | 'paciente' | 'atendimento' | 'usuario', acao, detalhe }   (log de atividades)

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
  const NIVEL_DO_PERFIL = { tecnico: 'Tecnico', enfermeiro: 'Enfermeiro', administrador: 'Administrador' };
  const usuarioDe = (p) => ({ id: p.id, nome: p.nome, perfil: PERFIL_DO_NIVEL[p.nivelAcesso] || 'tecnico' });
  const usuarioCompletoDe = (p) => ({ ...usuarioDe(p), coren: p.registroConselho, login: p.login, cpf: p.cpf });

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
  const itemDe = (item) => ({ codigo: item.codigo, descricao: item.descricao, valor: item.valor, tipo: item.tipo.toLowerCase() });

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

    async listarPacientes() {
      return (await chamar('GET', '/api/pacientes')).map(pacienteDe);
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
      return (await catalogoDaApi()).map(itemDe);
    },
    async listarProfissionais() {
      return (await chamar('GET', '/api/profissionais')).map((p) => ({ id: p.id, nome: p.nome, coren: p.registroConselho }));
    },

    // RF002/RF004/RF005 — usuários do sistema (cadastro só pelo Administrador)
    async listarUsuarios() {
      return (await chamar('GET', '/api/profissionais')).map(usuarioCompletoDe);
    },
    async cadastrarUsuario(dados) {
      return usuarioCompletoDe(await chamar('POST', '/api/profissionais', {
        nome: dados.nome, cpf: dados.cpf, registroConselho: dados.coren, nivelAcesso: NIVEL_DO_PERFIL[dados.perfil],
        login: dados.login, senha: dados.senha,
      }));
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

    // RF011 — atendimentos do paciente no mês (aaaa-mm) e o total calculado pelo servidor
    async consultarFechamento(pacienteId, mes) {
      const resposta = await chamar('GET', `/api/fechamento?paciente=${pacienteId}&mes=${mes}`);
      return { total: resposta.total, atendimentos: resposta.atendimentos.map(atendimentoDe) };
    },

    // RF014 — Configurações: dados gerais e tabela de procedimentos e materiais (alterações só do Administrador)
    async obterDadosGerais() {
      return chamar('GET', '/api/configuracoes');
    },
    async salvarDadosGerais(dados) {
      return chamar('PUT', '/api/configuracoes', dados);
    },
    async cadastrarItemCatalogo(dados) {
      const novo = await chamar('POST', '/api/catalogo', { ...dados, tipo: dados.tipo === 'material' ? 'Material' : 'Procedimento' });
      catalogo = null;   // a tabela mudou: é lida de novo na próxima consulta
      return itemDe(novo);
    },
    async atualizarItemCatalogo(codigo, dados) {
      const atual = (await catalogoDaApi()).find((item) => item.codigo === codigo);
      const salvo = await chamar('PUT', `/api/catalogo/${atual.id}`, { descricao: dados.descricao, valor: dados.valor });
      catalogo = null;
      return itemDe(salvo);
    },

    // RF013 — histórico de ações (só Administrador), mais recentes primeiro. O servidor registra sozinho
    async listarLog() {
      return (await chamar('GET', '/api/log')).map((r) => ({
        id: r.id, dataHora: r.dataHora, profissionalId: r.profissionalId, usuario: r.profissional || '—',
        tipo: r.tipo.toLowerCase(), acao: r.acao, detalhe: r.detalhe,
      }));
    },
  };
})();

const DadosFicticios = (() => {
  // No protótipo a pessoa é sempre a mesma; o perfil vem do painel "ver como"
  const USUARIO = { id: 1, nome: 'Thiago Fernandes' };

  const proximoId = (lista) => Math.max(0, ...lista.map((registro) => registro.id)) + 1;
  const maisRecentePrimeiro = (a, b) => b.data.localeCompare(a.data) || b.id - a.id;

  // ---------- Log de atividades (RF013) ----------
  // As ações feitas no protótipo ficam na sessão; as de exemplo estão em log.js
  function lerLogNovo() {
    try {
      return JSON.parse(sessionStorage.getItem('log-novos') || '[]');
    } catch {
      return [];
    }
  }

  function registrarNoLog(tipo, acao, detalhe) {
    const agora = new Date();
    const dd = (n) => String(n).padStart(2, '0');
    const dataHora = `${agora.getFullYear()}-${dd(agora.getMonth() + 1)}-${dd(agora.getDate())} ${agora.toTimeString().slice(0, 8)}`;
    const novos = lerLogNovo();
    novos.push({ id: 1000 + novos.length, dataHora, profissionalId: USUARIO.id, tipo, acao, detalhe });
    try {
      sessionStorage.setItem('log-novos', JSON.stringify(novos));
    } catch {}
  }

  // "a", "a e b", "a, b e c"
  const juntar = (partes) => (partes.length < 2 ? partes.join('')
    : `${partes.slice(0, -1).join(', ')} e ${partes[partes.length - 1]}`);

  function alteracoesPaciente(antes, depois) {
    const campos = [];
    if (antes.nome !== depois.nome) campos.push('nome');
    if (antes.nascimento !== depois.nascimento) campos.push('data de nascimento');
    if (antes.tipoDoc !== depois.tipoDoc || antes.documento !== depois.documento) campos.push('documento');
    if (['rua', 'complemento', 'bairro', 'cidade', 'uf'].some((c) => (antes[c] || '') !== (depois[c] || ''))) campos.push('endereço');
    return campos.length ? `alterou ${juntar(campos)}` : 'salvou sem alterações';
  }

  function resumoAtendimento(atendimento) {
    const paciente = carregarPacientes().find((p) => p.id === atendimento.pacienteId);
    const materiais = atendimento.itens.filter((item) => buscarItemCatalogo(item.codigo).tipo === 'material').length;
    const procedimentos = atendimento.itens.length - materiais;
    const partes = [];
    if (procedimentos) partes.push(`${procedimentos} procedimento${procedimentos > 1 ? 's' : ''}`);
    if (materiais) partes.push(`${materiais} ${materiais > 1 ? 'materiais' : 'material'}`);
    return `${paciente ? paciente.nome : 'Paciente'} — atendimento de ${dataParaTela(atendimento.data)}: ${juntar(partes)}`;
  }

  // ---------- Configurações (RF014) ----------
  const DADOS_GERAIS_INICIAIS = {
    nomeEmpresa: 'Cicatrizar Home Care', cnpj: '41.927.365/0001-08', telefone: '(67) 3325-4180',
    email: '', cidade: 'Campo Grande', uf: 'MS',
  };

  function lerDadosGerais() {
    try {
      const salvos = sessionStorage.getItem('dados-gerais');
      if (salvos) return JSON.parse(salvos);
    } catch {}
    return DADOS_GERAIS_INICIAIS;
  }

  const NOME_DO_DADO_GERAL = { nomeEmpresa: 'nome da empresa', cnpj: 'CNPJ', telefone: 'telefone', email: 'e-mail', cidade: 'cidade', uf: 'cidade' };
  const reais = (valor) => valor.toLocaleString('pt-BR', { style: 'currency', currency: 'BRL' });

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

    async listarPacientes() {
      return [...carregarPacientes()].sort((a, b) => a.nome.localeCompare(b.nome, 'pt-BR'));
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
      registrarNoLog('paciente', 'Cadastrou paciente', novo.nome);
      return novo;
    },
    async atualizarPaciente(id, dados) {
      const pacientes = carregarPacientes();
      const paciente = pacientes.find((p) => p.id === Number(id));
      if (!paciente) throw new ErroDoSistema('Paciente não encontrado.', 404);
      const antes = { ...paciente };
      Object.assign(paciente, dados);
      salvarPacientes(pacientes);
      registrarNoLog('paciente', 'Editou paciente', `${paciente.nome} — ${alteracoesPaciente(antes, paciente)}`);
      return paciente;
    },

    async listarCatalogo() {
      return carregarCatalogo();
    },
    async listarProfissionais() {
      return carregarProfissionais();
    },

    async listarUsuarios() {
      return [...carregarProfissionais()].sort((a, b) => a.nome.localeCompare(b.nome, 'pt-BR'));
    },
    // Confere repetições como o servidor. A senha não é guardada: no protótipo não há login de verdade
    async cadastrarUsuario({ senha, ...dados }) {
      const usuarios = carregarProfissionais();
      if (usuarios.some((u) => u.login === dados.login)) {
        throw new ErroDoSistema('Este login já está cadastrado. Escolha outro login.', 409);
      }
      if (usuarios.some((u) => soDigitos(u.cpf || '') === soDigitos(dados.cpf))) {
        throw new ErroDoSistema('Já existe um usuário com este CPF.', 409);
      }
      if (usuarios.some((u) => normalizar(u.coren) === normalizar(dados.coren))) {
        throw new ErroDoSistema('Já existe um usuário com este registro profissional.', 409);
      }
      const novo = { id: proximoId(usuarios), ...dados };
      usuarios.push(novo);
      salvarProfissionais(usuarios);
      registrarNoLog('usuario', 'Cadastrou usuário', `${novo.nome} (${PERFIS[novo.perfil].nome})`);
      return novo;
    },

    async obterDadosGerais() {
      return lerDadosGerais();
    },
    async salvarDadosGerais(dados) {
      const antes = lerDadosGerais();
      const campos = [...new Set(Object.keys(NOME_DO_DADO_GERAL)
        .filter((campo) => (antes[campo] || '') !== (dados[campo] || '')).map((campo) => NOME_DO_DADO_GERAL[campo]))];
      try {
        sessionStorage.setItem('dados-gerais', JSON.stringify(dados));
      } catch {}
      registrarNoLog('configuracao', 'Alterou dados gerais', campos.length ? `alterou ${juntar(campos)}` : 'salvou sem alterações');
      return dados;
    },
    async cadastrarItemCatalogo(dados) {
      const tabela = carregarCatalogo();
      if (tabela.some((item) => item.codigo === dados.codigo)) {
        throw new ErroDoSistema('Já existe um item com este código na tabela.', 409);
      }
      const novo = { codigo: dados.codigo, descricao: dados.descricao, valor: dados.valor, tipo: dados.tipo };
      salvarCatalogo([...tabela, novo]);
      registrarNoLog('configuracao', novo.tipo === 'material' ? 'Cadastrou material' : 'Cadastrou procedimento',
        `${novo.codigo} — ${novo.descricao} (${reais(novo.valor)})`);
      return novo;
    },
    async atualizarItemCatalogo(codigo, dados) {
      const tabela = carregarCatalogo().map((item) => ({ ...item }));
      const item = tabela.find((i) => i.codigo === codigo);
      if (!item) throw new ErroDoSistema('Item não encontrado na tabela.', 404);
      const mudancas = [];
      if (item.descricao !== dados.descricao) mudancas.push('descrição');
      if (item.valor !== dados.valor) mudancas.push(`valor de ${reais(item.valor)} para ${reais(dados.valor)}`);
      Object.assign(item, { descricao: dados.descricao, valor: dados.valor });
      salvarCatalogo(tabela);
      registrarNoLog('configuracao', item.tipo === 'material' ? 'Alterou material' : 'Alterou procedimento',
        `${codigo} — ${mudancas.length ? `alterou ${juntar(mudancas)}` : 'salvou sem alterações'}`);
      return item;
    },

    async listarLog() {
      const profissionais = carregarProfissionais();
      return [...LOG_INICIAL, ...lerLogNovo()]
        .map((registro) => ({ ...registro, usuario: profissionais.find((p) => p.id === registro.profissionalId)?.nome || '—' }))
        .sort((a, b) => b.dataHora.localeCompare(a.dataHora) || b.id - a.id);
    },

    async listarAtendimentos(pacienteId) {
      return carregarAtendimentos().filter((atendimento) => atendimento.pacienteId === pacienteId).sort(maisRecentePrimeiro);
    },
    async registrarAtendimento(dados) {
      const atendimentos = carregarAtendimentos();
      const novo = { id: proximoId(atendimentos), registradoPor: USUARIO.id, ...dados };
      atendimentos.push(novo);
      salvarAtendimentos(atendimentos);
      registrarNoLog('atendimento', 'Registrou atendimento', resumoAtendimento(novo));
      return novo;
    },
    async atualizarAtendimento(id, dados) {
      const atendimentos = carregarAtendimentos();
      const atendimento = atendimentos.find((a) => a.id === id);
      if (!atendimento) throw new ErroDoSistema('Atendimento não encontrado.', 404);
      Object.assign(atendimento, dados);
      salvarAtendimentos(atendimentos);
      registrarNoLog('atendimento', 'Editou atendimento', resumoAtendimento(atendimento));
      return atendimento;
    },

    async consultarFechamento(pacienteId, mes) {
      const doMes = carregarAtendimentos()
        .filter((a) => a.pacienteId === pacienteId && a.data.startsWith(mes))
        .sort((a, b) => a.data.localeCompare(b.data) || a.id - b.id);
      const total = doMes.flatMap((a) => a.itens)
        .reduce((soma, item) => soma + item.quantidade * buscarItemCatalogo(item.codigo).valor, 0);
      return { total, atendimentos: doMes };
    },
  };
})();

const Dados = MODO_API ? DadosApi : DadosFicticios;
