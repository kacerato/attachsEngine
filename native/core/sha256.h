#pragma once
#include "core/base.h"
#include <array>
#include <span>
#include <string>

namespace ae {
// SHA-256 (FIPS 180-4), escrito aqui em vez de puxado de uma dependência.
//
// O openssl vendorizado existe só no alvo Android e o registro de recursos
// precisa do mesmo hash no host, nos testes e no aparelho — um hash que muda de
// implementação entre plataformas não serve para dizer "este arquivo é o mesmo".
// São 60 linhas de aritmética fixa, sem alocação e sem estado global.
//
// Uso: identidade de CONTEÚDO. Ele não é a identidade do recurso — essa é o
// GUID, que não muda quando o conteúdo muda.
class Sha256 final {
public:
  void update(std::span<const u8> data) {
    for (auto byte : data) {
      buffer_[used_++] = byte;
      if (used_ == 64) { compress(); used_ = 0; }
    }
    length_ += data.size();
  }
  // Fecha o hash. Chamar de novo depois disto devolveria outro valor, então o
  // estado é marcado: um digest tomado duas vezes é erro de quem chama.
  std::array<u8, 32> digest() {
    const u64 bits = length_ * 8;
    update(std::span<const u8>(&kPadding, 1));
    while (used_ != 56) update(std::span<const u8>(&kZero, 1));
    u8 tail[8];
    for (u32 i = 0; i < 8; ++i) tail[i] = static_cast<u8>(bits >> (56 - i * 8));
    update(std::span<const u8>(tail, 8));
    std::array<u8, 32> out{};
    for (u32 i = 0; i < 8; ++i)
      for (u32 j = 0; j < 4; ++j) out[i * 4 + j] = static_cast<u8>(state_[i] >> (24 - j * 8));
    return out;
  }
  static std::string hex(std::span<const u8> data) {
    Sha256 hash;
    hash.update(data);
    return hex(hash.digest());
  }
  static std::string hex(const std::array<u8, 32> &digest) {
    static const char *digits = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (auto byte : digest) { out.push_back(digits[byte >> 4]); out.push_back(digits[byte & 15]); }
    return out;
  }

private:
  static constexpr u8 kPadding = 0x80, kZero = 0;
  static u32 rotate(u32 value, u32 bits) { return (value >> bits) | (value << (32 - bits)); }
  void compress() {
    static constexpr u32 k[64]{
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    u32 w[64];
    for (u32 i = 0; i < 16; ++i)
      w[i] = (static_cast<u32>(buffer_[i * 4]) << 24) | (static_cast<u32>(buffer_[i * 4 + 1]) << 16) |
             (static_cast<u32>(buffer_[i * 4 + 2]) << 8) | static_cast<u32>(buffer_[i * 4 + 3]);
    for (u32 i = 16; i < 64; ++i) {
      const u32 s0 = rotate(w[i - 15], 7) ^ rotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const u32 s1 = rotate(w[i - 2], 17) ^ rotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    u32 e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (u32 i = 0; i < 64; ++i) {
      const u32 s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
      const u32 choose = (e & f) ^ (~e & g);
      const u32 temp1 = h + s1 + choose + k[i] + w[i];
      const u32 s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
      const u32 majority = (a & b) ^ (a & c) ^ (b & c);
      const u32 temp2 = s0 + majority;
      h = g; g = f; f = e; e = d + temp1;
      d = c; c = b; b = a; a = temp1 + temp2;
    }
    const u32 add[8]{a, b, c, d, e, f, g, h};
    for (u32 i = 0; i < 8; ++i) state_[i] += add[i];
  }
  u32 state_[8]{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  u8 buffer_[64]{};
  u32 used_ = 0;
  u64 length_ = 0;
};
} // namespace ae
