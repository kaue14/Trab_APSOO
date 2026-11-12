// Funções de formulário usadas em mais de uma tela (números e datas)

const soDigitos = (texto) => texto.replace(/\D/g, '');

// Ignora acentos e maiúsculas nas comparações: "jose" encontra "José"
const normalizar = (texto) => texto.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().trim();

// O usuário digita só números e a tela coloca as barras: 15081948 → 15/08/1948
function mascaraData(valor) {
  const d = soDigitos(valor).slice(0, 8);
  return [d.slice(0, 2), d.slice(2, 4), d.slice(4)].filter(Boolean).join('/');
}

// Valida uma data dd/mm/aaaa (NF002): devolve a mensagem de erro, ou '' se estiver certa
function validarData(valor, nomeDoCampo) {
  if (!valor) return `Informe a ${nomeDoCampo}.`;
  const partes = /^(\d{2})\/(\d{2})\/(\d{4})$/.exec(valor);
  if (!partes) return 'Use o formato dd/mm/aaaa.';
  const [dia, mes, ano] = partes.slice(1).map(Number);
  const data = new Date(ano, mes - 1, dia);
  if (data.getDate() !== dia || data.getMonth() !== mes - 1) return 'Data inválida.';
  if (data > new Date()) return `A ${nomeDoCampo} não pode estar no futuro.`;
  if (ano < 1900) return 'Confira o ano informado.';
  return '';
}

// Converte entre o formato da tela (dd/mm/aaaa) e o guardado (aaaa-mm-dd)
const dataParaTela = (iso) => iso.split('-').reverse().join('/');
const dataParaGuardar = (texto) => texto.split('/').reverse().join('-');
