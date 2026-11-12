// Comportamento comum às telas internas do protótipo (menu por perfil, avisos e Sair)

const PERFIS = {
  tecnico: {
    nome: 'Técnico de Enfermagem',
    descricao: 'Você pode buscar pacientes e registrar os procedimentos e materiais dos atendimentos.',
  },
  enfermeiro: {
    nome: 'Enfermeiro',
    descricao: 'Além do registro dos atendimentos, você pode cadastrar pacientes e consultar o fechamento mensal.',
  },
  administrador: {
    nome: 'Administrador',
    descricao: 'Você tem acesso a todas as funções, incluindo usuários, log de atividades e configurações.',
  },
};

// Mostra só o que o perfil pode acessar (UC001, passo 4)
function aplicarPerfil(perfil) {
  perfilAtual = perfil;
  document.querySelectorAll('[data-perfis]').forEach((elemento) => {
    elemento.hidden = !elemento.dataset.perfis.split(' ').includes(perfil);
  });
  document.getElementById('perfil-nome').textContent = PERFIS[perfil].nome;
  const descricao = document.getElementById('perfil-descricao');
  if (descricao) descricao.textContent = PERFIS[perfil].descricao;
  try {
    sessionStorage.setItem('perfil', perfil);
  } catch {}
  // Avisa a tela, para ela atualizar o que depende do perfil além do data-perfis
  document.dispatchEvent(new CustomEvent('perfilalterado'));
}

let perfilAtual = 'tecnico';
try {
  perfilAtual = sessionStorage.getItem('perfil') || perfilAtual;
} catch {}
document.querySelector(`input[name="perfil"][value="${perfilAtual}"]`).checked = true;
aplicarPerfil(perfilAtual);

document.querySelectorAll('input[name="perfil"]').forEach((opcao) => {
  opcao.addEventListener('change', () => aplicarPerfil(opcao.value));
});

// Aviso rápido no rodapé da área principal
const aviso = document.getElementById('aviso');
let temporizadorAviso;

function mostrarAviso(texto) {
  aviso.textContent = texto;
  aviso.hidden = false;
  clearTimeout(temporizadorAviso);
  temporizadorAviso = setTimeout(() => { aviso.hidden = true; }, 2500);
}

// Telas que ainda não existem no protótipo
document.querySelectorAll('.janela a[href="#"]').forEach((link) => {
  link.addEventListener('click', (evento) => {
    evento.preventDefault();
    const nome = (link.querySelector('strong') || link).textContent.trim();
    mostrarAviso(`A tela “${nome}” será desenhada nas próximas etapas.`);
  });
});

// UC003 — Encerrar sessão: invalida a sessão e volta para o login
document.getElementById('sair').addEventListener('click', () => {
  try {
    sessionStorage.removeItem('perfil');
  } catch {}
});
