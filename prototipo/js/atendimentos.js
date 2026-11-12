// Profissionais e atendimentos do protótipo. Um atendimento é uma visita (paciente + data +
// profissional) com os itens lançados — como Atendimento e AtendimentoItem no diagrama de classes.

const PROFISSIONAIS = [
  { id: 1, nome: 'Thiago Fernandes', coren: 'COREN-MS 401.227' },
  { id: 2, nome: 'Ana Paula Lima', coren: 'COREN-MS 512.334' },
  { id: 3, nome: 'Carlos Eduardo Mendes', coren: 'COREN-MS 498.120' },
];

// Quem está usando o sistema no protótipo (o perfil muda pelo painel, a pessoa não)
const USUARIO_LOGADO = 1;

const buscarProfissional = (id) => PROFISSIONAIS.find((profissional) => profissional.id === id);

// Datas relativas a hoje, para o histórico sempre parecer recente
function diasAtras(dias) {
  const data = new Date();
  data.setDate(data.getDate() - dias);
  const mes = String(data.getMonth() + 1).padStart(2, '0');
  const dia = String(data.getDate()).padStart(2, '0');
  return `${data.getFullYear()}-${mes}-${dia}`;
}

const ATENDIMENTOS_INICIAIS = [
  {
    id: 1, pacienteId: 2, data: diasAtras(1), profissionalId: 2, registradoPor: 2,
    itens: [
      { codigo: '60090022', quantidade: 1 }, { codigo: '88040020', quantidade: 1 },
      { codigo: '0071000417', quantidade: 10 }, { codigo: '0071000219', quantidade: 1 },
      { codigo: '0071000021', quantidade: 2 }, { codigo: '0071000126', quantidade: 4 },
    ],
  },
  {
    id: 2, pacienteId: 2, data: diasAtras(4), profissionalId: 1, registradoPor: 1,
    itens: [
      { codigo: '60090022', quantidade: 1 }, { codigo: '88040020', quantidade: 1 },
      { codigo: '0071000417', quantidade: 10 }, { codigo: '0071000219', quantidade: 1 },
      { codigo: '0071000126', quantidade: 4 },
    ],
  },
  {
    id: 3, pacienteId: 1, data: diasAtras(2), profissionalId: 3, registradoPor: 3,
    itens: [
      { codigo: '50000535', quantidade: 1 }, { codigo: '88040445', quantidade: 1 },
      { codigo: '0071000126', quantidade: 2 }, { codigo: '0071000553', quantidade: 1 },
    ],
  },
  {
    id: 4, pacienteId: 7, data: diasAtras(3), profissionalId: 1, registradoPor: 1,
    itens: [{ codigo: '88040429', quantidade: 1 }, { codigo: '0071000175', quantidade: 1 }],
  },
  {
    id: 5, pacienteId: 7, data: diasAtras(8), profissionalId: 2, registradoPor: 2,
    itens: [
      { codigo: '88040160', quantidade: 1 }, { codigo: '0071000182', quantidade: 1 },
      { codigo: '0071000069', quantidade: 1 }, { codigo: '0071000126', quantidade: 2 },
    ],
  },
];

function carregarAtendimentos() {
  try {
    const salvos = sessionStorage.getItem('atendimentos');
    if (salvos) return JSON.parse(salvos);
  } catch {}
  return ATENDIMENTOS_INICIAIS;
}

function salvarAtendimentos(atendimentos) {
  try {
    sessionStorage.setItem('atendimentos', JSON.stringify(atendimentos));
  } catch {}
}
