Compilar:
  gcc -c sqlite3.c -o sqlite3.o
  g++ main.cpp sqlite3.o -o sistema_homecare

Sistema com telas (servidor web + telas da pasta prototipo/):
  g++ -std=c++17 -static servidor/servidor.cpp sqlite3.o -o cicatrizar -lws2_32 -lbcrypt

Rodar, na pasta do projeto:
  cicatrizar                  abre o sistema numa janela própria (Edge em modo aplicativo)
  cicatrizar --sem-janela     só inicia o servidor; acesse http://localhost:8080
  cicatrizar outro-banco.db   usa outro arquivo de banco (bom para testes)

Primeiro acesso:
  Se nenhum usuário tiver senha, o servidor cria o administrador inicial.
  Login: admin
  Senha: cicatrizar
  Depois de entrar, cadastre os usuários de verdade.

Ao abrir o banco, as colunas novas (endereço e carteira do paciente; login, CPF e senha
dos profissionais; quem registrou o atendimento) são criadas automaticamente, sem perder dados.
