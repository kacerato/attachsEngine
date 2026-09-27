// Expressões em campo numérico do Inspector.
//
// Unity 6000.0 Manual/InspectorNumericFields e ScriptReference/ExpressionEvaluator:
// o campo aceita uma conta em vez de um número — `2*3`, `(1+2)^2`, `sqrt(2)`,
// `sin(pi/4)*5` —, uma edição relativa ao valor atual — `+=5`, `-=1`, `*=2`,
// `/=4` — e as distribuições `L(a,b)` (linear de a até b entre os objetos
// editados juntos) e `R(a,b)` (aleatório entre a e b para cada um). Funções
// aninham: `cos(L(0,2*pi))*5`.
//
// Operadores: `+ - * / % ^`, com `^` à direita e acima de `* / %`; menos
// unário acima de tudo exceto `^` (`-2^2` é -4, como na matemática). Funções:
// sqrt, floor, ceil, round, sin, cos, tan (radianos); constante `pi`.
//
// O teclado do Android pode trazer vírgula decimal. Sem parênteses a vírgula é
// decimal (`2,5`); dentro de uma função ela separa argumentos (`L(0,2)`), e
// então o decimal é o ponto. Divisão por zero, raiz negativa e qualquer
// resultado não finito recusam a expressão em vez de gravar infinito.
#pragma once
#include "core/base.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

namespace ae::editor {

struct NumericExpressionContext {
  double current = 0;   // valor do campo antes da edição (+=, -=, *=, /=)
  u32 index = 0;        // posição deste objeto entre os editados juntos
  u32 count = 1;        // quantos objetos a edição alcança
  u64 seed = 0;         // semente de R(a,b); a mesma semente repete o sorteio
};

namespace detail {
class NumericExpressionParser {
public:
  NumericExpressionParser(std::string_view text, const NumericExpressionContext &context)
      : text_(text), context_(context) {}
  bool parse(double &out, std::string &error) {
    if (!expression(out)) { error = error_.empty() ? "Expressão inválida" : error_; return false; }
    skip();
    if (at_ != text_.size()) { error = "Sobra texto depois da expressão"; return false; }
    if (!std::isfinite(out)) { error = "O resultado não é um número finito"; return false; }
    return true;
  }

private:
  void skip() { while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t')) ++at_; }
  bool eat(char c) { skip(); if (at_ < text_.size() && text_[at_] == c) { ++at_; return true; } return false; }
  bool fail(const char *message) { if (error_.empty()) error_ = message; return false; }

  bool expression(double &out) {
    if (!term(out)) return false;
    for (;;) {
      if (eat('+')) { double r; if (!term(r)) return false; out += r; }
      else if (eat('-')) { double r; if (!term(r)) return false; out -= r; }
      else return true;
    }
  }
  bool term(double &out) {
    if (!unary(out)) return false;
    for (;;) {
      if (eat('*')) { double r; if (!unary(r)) return false; out *= r; }
      else if (eat('/')) { double r; if (!unary(r)) return false; if (r == 0) return fail("Divisão por zero"); out /= r; }
      else if (eat('%')) { double r; if (!unary(r)) return false; if (r == 0) return fail("Resto por zero"); out = std::fmod(out, r); }
      else return true;
    }
  }
  bool unary(double &out) {
    if (eat('-')) { if (!unary(out)) return false; out = -out; return true; }
    if (eat('+')) return unary(out);
    return power(out);
  }
  bool power(double &out) {
    if (!primary(out)) return false;
    if (eat('^')) {
      double exponent;
      if (!unary(exponent)) return false;
      out = std::pow(out, exponent);
      if (!std::isfinite(out)) return fail("Potência sem resultado real");
    }
    return true;
  }
  bool primary(double &out) {
    skip();
    if (at_ >= text_.size()) return fail("Expressão incompleta");
    const char c = text_[at_];
    if (c == '(') { ++at_; if (!expression(out)) return false; return eat(')') || fail("Falta fechar parêntese"); }
    if ((c >= '0' && c <= '9') || c == '.' || (c == ',' && !depth_)) return number(out);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return call(out);
    return fail("Símbolo inesperado");
  }
  bool number(double &out) {
    usize end = at_;
    bool point = false, digits = false;
    std::string value;
    while (end < text_.size()) {
      const char c = text_[end];
      if (c >= '0' && c <= '9') { value += c; digits = true; }
      else if ((c == '.' || (c == ',' && !depth_)) && !point) { value += '.'; point = true; }
      else break;
      ++end;
    }
    if (!digits) return fail("Número inválido");
    // Expoente científico, como o valor que o próprio campo mostra (1e-05).
    if (end < text_.size() && (text_[end] == 'e' || text_[end] == 'E')) {
      usize exponent = end + 1;
      std::string suffix = "e";
      if (exponent < text_.size() && (text_[exponent] == '+' || text_[exponent] == '-')) suffix += text_[exponent++];
      const usize first = exponent;
      while (exponent < text_.size() && text_[exponent] >= '0' && text_[exponent] <= '9') suffix += text_[exponent++];
      if (exponent > first) { value += suffix; end = exponent; }
    }
    char *stop = nullptr;
    out = std::strtod(value.c_str(), &stop);
    if (!stop || *stop) return fail("Número inválido");
    at_ = end;
    return true;
  }
  bool call(double &out) {
    usize end = at_;
    while (end < text_.size() && ((text_[end] >= 'a' && text_[end] <= 'z') || (text_[end] >= 'A' && text_[end] <= 'Z'))) ++end;
    std::string name(text_.substr(at_, end - at_));
    for (auto &c : name) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    at_ = end;
    if (name == "pi") { out = 3.14159265358979323846; return true; }
    if (!eat('(')) return fail("Função sem parênteses");
    ++depth_;
    double a = 0, b = 0;
    if (!expression(a)) return false;
    const bool pair = name == "l" || name == "r";
    if (pair && (!eat(',') || !expression(b))) return fail("L e R pedem dois valores: L(a,b)");
    --depth_;
    if (!eat(')')) return fail("Falta fechar parêntese");
    if (name == "sqrt") { if (a < 0) return fail("Raiz de número negativo"); out = std::sqrt(a); }
    else if (name == "floor") out = std::floor(a);
    else if (name == "ceil") out = std::ceil(a);
    else if (name == "round") out = std::round(a);
    else if (name == "sin") out = std::sin(a);
    else if (name == "cos") out = std::cos(a);
    else if (name == "tan") out = std::tan(a);
    else if (name == "l") {
      // Um objeto só recebe o começo da faixa; N objetos vão de a até b.
      out = context_.count > 1 ? a + (b - a) * context_.index / (context_.count - 1) : a;
    } else if (name == "r") {
      u64 z = context_.seed + 0x9E3779B97F4A7C15ull * (static_cast<u64>(context_.index) + 1);
      z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
      z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
      z ^= z >> 31;
      out = a + (b - a) * (static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0));
    } else return fail("Função desconhecida");
    return true;
  }

  std::string_view text_;
  const NumericExpressionContext &context_;
  usize at_ = 0;
  u32 depth_ = 0;
  std::string error_;
};
} // namespace detail

inline bool evaluateNumericExpression(std::string_view text, const NumericExpressionContext &context,
                                      double &out, std::string *error = nullptr) {
  std::string message;
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
  if (text.empty()) { if (error) *error = "Campo vazio"; return false; }
  char relative = 0;
  if (text.size() >= 2 && text[1] == '=' && (text[0] == '+' || text[0] == '-' || text[0] == '*' || text[0] == '/')) {
    relative = text[0];
    text.remove_prefix(2);
  }
  double value = 0;
  if (!detail::NumericExpressionParser(text, context).parse(value, message)) { if (error) *error = message; return false; }
  switch (relative) {
    case '+': value = context.current + value; break;
    case '-': value = context.current - value; break;
    case '*': value = context.current * value; break;
    case '/':
      if (value == 0) { if (error) *error = "Divisão por zero"; return false; }
      value = context.current / value;
      break;
    default: break;
  }
  if (!std::isfinite(value)) { if (error) *error = "O resultado não é um número finito"; return false; }
  out = value;
  return true;
}

} // namespace ae::editor
