#ifndef SEGURANCA_H
#define SEGURANCA_H

// NF004 — hash SHA-256 das senhas e sorteio dos códigos de sessão, com a API de criptografia
// do próprio Windows (bcrypt.h). Compilar com -lbcrypt.

#include <windows.h>
#include <bcrypt.h>
#include <string>

namespace Seguranca {

    inline std::string paraHexadecimal(const unsigned char* bytes, size_t tamanho) {
        static const char digitos[] = "0123456789abcdef";
        std::string texto;
        for (size_t i = 0; i < tamanho; i++) {
            texto += digitos[bytes[i] >> 4];
            texto += digitos[bytes[i] & 0x0F];
        }
        return texto;
    }

    // Hash SHA-256 de um texto, em hexadecimal (64 caracteres)
    inline std::string sha256(const std::string& texto) {
        unsigned char hash[32] = {0};
        BCRYPT_ALG_HANDLE algoritmo = nullptr;
        BCRYPT_HASH_HANDLE calculo = nullptr;

        if (BCryptOpenAlgorithmProvider(&algoritmo, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
            if (BCryptCreateHash(algoritmo, &calculo, nullptr, 0, nullptr, 0, 0) == 0) {
                BCryptHashData(calculo, (PUCHAR)texto.data(), (ULONG)texto.size(), 0);
                BCryptFinishHash(calculo, hash, sizeof(hash), 0);
                BCryptDestroyHash(calculo);
            }
            BCryptCloseAlgorithmProvider(algoritmo, 0);
        }
        return paraHexadecimal(hash, sizeof(hash));
    }

    // Código aleatório e imprevisível que identifica uma sessão aberta (RF001 / RF003)
    inline std::string gerarCodigoSessao() {
        unsigned char bytes[24] = {0};
        BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        return paraHexadecimal(bytes, sizeof(bytes));
    }
}

#endif
