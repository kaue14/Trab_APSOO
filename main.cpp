#include <iostream>
#include "sqlite3.h"

static int callback_impressao(void* data, int argc, char** argv, char** azColName) {
    for(int i = 0; i < argc; i++) {
        std::cout << azColName[i] << ": " << (argv[i] ? argv[i] : "NULL") << " | ";
    }
    std::cout << "\n";
    return 0;
}

int main() {
    sqlite3* db;
    char* mensagemErro = 0;
    int status;

    status = sqlite3_open("homecare.db", &db);

    if(status) {
        std::cerr << "Erro ao abrir o banco de dados: " << sqlite3_errmsg(db) << std::endl;
        return 0;
    }

    std::string sql = "SELECT codigo, descricao, valor FROM catalogo_itens";
    status = sqlite3_exec(db, sql.c_str(), callback_impressao, 0, &mensagemErro);

    if(status != SQLITE_OK) {
        std::cerr << "Erro na execucao do SQL: " << mensagemErro << "\n";
        sqlite3_free(mensagemErro);
    }

    sqlite3_close(db);

    return 0;
}
