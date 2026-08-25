// Span<T> do Aether.
//
// Decisão de design: o enunciado original pedia um Span<T> escrito à mão para
// controlar o binário gerado, mas C++20 já traz std::span na biblioteca
// padrão como um wrapper (ponteiro + tamanho) totalmente header-only e sem
// custo de runtime (nenhuma alocação, nenhuma vtable, inlining trivial) — ou
// seja, não existe binário "extra" para controlar. Reimplementar a mesma
// coisa à mão só adicionaria superfície de bug sem nenhum ganho de
// desempenho ou de tamanho de código. Por isso usamos std::span diretamente
// e só oferecemos um alias no namespace do motor para manter o estilo de
// nomenclatura do resto da engine.
#pragma once

#include <span>

namespace ae {

template <typename T> using Span = std::span<T>;

} // namespace ae
