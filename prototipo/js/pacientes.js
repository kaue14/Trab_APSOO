// "Banco de dados" de pacientes do protótipo: começa com uma lista fictícia e guarda
// cadastros e edições na sessão do navegador (some ao fechar a aba)

const PACIENTES_INICIAIS = [
  { id: 1, nome: 'Antônio Carlos Ferreira', nascimento: '1941-03-12', tipoDoc: 'CPF', documento: '318.452.907-14', rua: 'Rua 14 de Julho, 1520', bairro: 'Centro' },
  { id: 2, nome: 'Benedita Alves da Silva', nascimento: '1938-08-27', tipoDoc: 'Carteira', documento: '0016 4521 8830 2217', rua: 'Rua Bahia, 845', bairro: 'Jardim dos Estados' },
  { id: 3, nome: 'Cláudio Roberto Nunes', nascimento: '1957-11-05', tipoDoc: 'CPF', documento: '402.118.653-90', rua: 'Av. Mato Grosso, 3200', bairro: 'Carandá Bosque' },
  { id: 4, nome: 'Dalva Pereira Gomes', nascimento: '1946-01-19', tipoDoc: 'CPF', documento: '251.774.320-05', rua: 'Rua Joaquim Murtinho, 410', bairro: 'Centro' },
  { id: 5, nome: 'Francisco das Chagas Lima', nascimento: '1950-06-30', tipoDoc: 'Carteira', documento: '0016 7782 1045 9963', rua: 'Rua Brilhante, 77', bairro: 'Vila Bandeirantes' },
  { id: 6, nome: 'Helena Souza Martins', nascimento: '1962-09-14', tipoDoc: 'CPF', documento: '587.203.441-28', rua: 'Rua Spipe Calarge, 1200', bairro: 'Tiradentes' },
  { id: 7, nome: 'José Aparecido da Silva', nascimento: '1944-04-02', tipoDoc: 'CPF', documento: '146.920.385-61', rua: 'Rua Marquês de Pombal, 500', bairro: 'Tiradentes' },
  { id: 8, nome: 'Maria Aparecida Rocha', nascimento: '1935-12-23', tipoDoc: 'Carteira', documento: '0016 3390 5571 0482', rua: 'Av. Afonso Pena, 4100', bairro: 'Chácara Cachoeira' },
].map((paciente) => ({ complemento: '', cidade: 'Campo Grande', uf: 'MS', ...paciente }));

function carregarPacientes() {
  try {
    const salvos = sessionStorage.getItem('pacientes');
    if (salvos) return JSON.parse(salvos);
  } catch {}
  return PACIENTES_INICIAIS;
}

function salvarPacientes(pacientes) {
  try {
    sessionStorage.setItem('pacientes', JSON.stringify(pacientes));
  } catch {}
}

const soDigitos = (texto) => texto.replace(/\D/g, '');
