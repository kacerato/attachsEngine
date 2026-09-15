#pragma once
#include "core/base.h"
#include "resources/image_decode.h"
#include <span>
#include <string>
#include <vector>

namespace ae::resources {
// Imagens embutidas de um GLB (R4, T02), com os bytes ORIGINAIS do arquivo.
//
// Extrair uma textura para o projeto não recodifica nada: o PNG ou JPEG que o
// autor pôs no GLB é o mesmo arquivo que vai para `Texturas/`. Só imagens em
// bufferView do bloco binário; URI externa não chega aqui, porque o projeto
// guarda o `.gltf` já empacotado (Entrega 4).
struct GlbEmbeddedImage {
  u32 index = 0;          // posição em "images"
  std::string name;       // "name" do arquivo, vazio quando não há
  ImageContainer container = ImageContainer::Unknown;
  std::vector<u8> bytes;
};

// Falha fechada no contêiner (cabeçalho, blocos, JSON); uma imagem individual
// com intervalo inválido fica de fora sem derrubar as outras.
bool listGlbEmbeddedImages(std::span<const u8> glb, std::vector<GlbEmbeddedImage> &out, std::string &diagnostic);
} // namespace ae::resources
