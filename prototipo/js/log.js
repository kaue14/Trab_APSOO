// Log de atividades do protótipo (RF011): ações de exemplo, combinando com os atendimentos de
// atendimentos.js. O que for feito no protótipo entra também (veja registrarNoLog em dados.js).
// Precisa de formulario.js e atendimentos.js carregados antes.

const quando = (dias, hora) => `${diasAtras(dias)} ${hora}:00`;
const diaNaTela = (dias) => dataParaTela(diasAtras(dias));

const LOG_INICIAL = [
  { id: 1, dataHora: quando(9, '08:02'), profissionalId: 1, tipo: 'acesso', acao: 'Entrou no sistema', detalhe: 'Perfil Administrador' },
  { id: 2, dataHora: quando(9, '08:10'), profissionalId: 1, tipo: 'usuario', acao: 'Cadastrou usuário', detalhe: 'Carlos Eduardo Mendes (Técnico de Enfermagem)' },
  { id: 3, dataHora: quando(9, '08:31'), profissionalId: 1, tipo: 'acesso', acao: 'Saiu do sistema', detalhe: '' },
  { id: 4, dataHora: quando(8, '07:55'), profissionalId: 2, tipo: 'acesso', acao: 'Entrou no sistema', detalhe: 'Perfil Enfermeiro' },
  { id: 5, dataHora: quando(8, '10:40'), profissionalId: 2, tipo: 'atendimento', acao: 'Registrou atendimento',
    detalhe: `José Aparecido da Silva — atendimento de ${diaNaTela(8)}: 1 procedimento e 3 materiais` },
  { id: 6, dataHora: quando(6, '14:20'), profissionalId: 2, tipo: 'paciente', acao: 'Editou paciente', detalhe: 'Maria Aparecida Rocha — alterou endereço' },
  { id: 7, dataHora: quando(4, '08:05'), profissionalId: 1, tipo: 'acesso', acao: 'Entrou no sistema', detalhe: 'Perfil Administrador' },
  { id: 8, dataHora: quando(4, '11:30'), profissionalId: 1, tipo: 'atendimento', acao: 'Registrou atendimento',
    detalhe: `Benedita Alves da Silva — atendimento de ${diaNaTela(4)}: 2 procedimentos e 3 materiais` },
  { id: 9, dataHora: quando(3, '10:02'), profissionalId: 1, tipo: 'atendimento', acao: 'Registrou atendimento',
    detalhe: `José Aparecido da Silva — atendimento de ${diaNaTela(3)}: 1 procedimento e 1 material` },
  { id: 10, dataHora: quando(3, '16:30'), profissionalId: 1, tipo: 'paciente', acao: 'Cadastrou paciente', detalhe: 'Helena Souza Martins' },
  { id: 11, dataHora: quando(3, '17:05'), profissionalId: 1, tipo: 'acesso', acao: 'Saiu do sistema', detalhe: '' },
  { id: 12, dataHora: quando(2, '07:50'), profissionalId: 3, tipo: 'acesso', acao: 'Entrou no sistema', detalhe: 'Perfil Técnico de Enfermagem' },
  { id: 13, dataHora: quando(2, '09:25'), profissionalId: 3, tipo: 'atendimento', acao: 'Registrou atendimento',
    detalhe: `Antônio Carlos Ferreira — atendimento de ${diaNaTela(2)}: 2 procedimentos e 2 materiais` },
  { id: 14, dataHora: quando(2, '09:40'), profissionalId: 3, tipo: 'atendimento', acao: 'Editou atendimento',
    detalhe: `Antônio Carlos Ferreira — atendimento de ${diaNaTela(2)}: 2 procedimentos e 2 materiais` },
  { id: 15, dataHora: quando(1, '08:00'), profissionalId: 2, tipo: 'acesso', acao: 'Entrou no sistema', detalhe: 'Perfil Enfermeiro' },
  { id: 16, dataHora: quando(1, '09:12'), profissionalId: 2, tipo: 'atendimento', acao: 'Registrou atendimento',
    detalhe: `Benedita Alves da Silva — atendimento de ${diaNaTela(1)}: 2 procedimentos e 4 materiais` },
];
