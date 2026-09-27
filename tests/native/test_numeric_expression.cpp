#include "harness.h"
#include "editor/editor_numeric_expression.h"

#include <cmath>
#include <string>

using namespace ae;
using namespace ae::editor;

namespace {
double eval(const char *text, NumericExpressionContext context = {}) {
  double out = -12345;
  std::string error;
  return evaluateNumericExpression(text, context, out, &error) ? out : std::nan("");
}
bool refused(const char *text, NumericExpressionContext context = {}) {
  double out = 0;
  return !evaluateNumericExpression(text, context, out);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
} // namespace

// Unity 6000.0 ScriptReference/ExpressionEvaluator: + - * / % ^, parênteses,
// sqrt floor ceil round sin cos tan e pi.
AE_TEST(numeric_expression_follows_the_unity_evaluator_operators) {
  AE_EXPECT_TRUE(near(eval("2*3"), 6), "produto");
  AE_EXPECT_TRUE(near(eval("1+2*3"), 7), "precedência de * sobre +");
  AE_EXPECT_TRUE(near(eval("(1+2)^2"), 9), "parênteses e potência");
  AE_EXPECT_TRUE(near(eval("2^3^2"), 512), "potência associa à direita");
  AE_EXPECT_TRUE(near(eval("-2^2"), -4), "menos unário abaixo da potência");
  AE_EXPECT_TRUE(near(eval("10%4"), 2), "resto");
  AE_EXPECT_TRUE(near(eval("7/2"), 3.5), "divisão real");
  AE_EXPECT_TRUE(near(eval("sqrt(16)+floor(2.7)+ceil(2.1)+round(2.5)"), 4 + 2 + 3 + 3), "funções");
  AE_EXPECT_TRUE(near(eval("sin(pi/2)*5"), 5), "trigonometria em radianos e pi");
  AE_EXPECT_TRUE(near(eval(" 1e-5 "), 1e-5), "o valor que o campo mostra volta a ser aceito");
  AE_EXPECT_TRUE(near(eval("2,5"), 2.5), "vírgula decimal do teclado do Android");
  AE_EXPECT_TRUE(refused("1/0") && refused("sqrt(-1)") && refused("2+") && refused("abc(1)") && refused("(1") &&
                 refused("") && refused("1 2"), "entrada inválida é recusada, nunca vira infinito ou zero");
}

// Unity Manual/InspectorNumericFields: +=, -=, *=, /= sobre o valor atual e as
// distribuições L(a,b) e R(a,b), aninháveis.
AE_TEST(numeric_expression_relative_edits_and_distributions) {
  NumericExpressionContext context;
  context.current = 3;
  AE_EXPECT_TRUE(near(eval("+=5", context), 8) && near(eval("-=1", context), 2) &&
                 near(eval("*=2", context), 6) && near(eval("/=4", context), .75), "relativos ao valor aberto");
  AE_EXPECT_TRUE(refused("/=0", context), "divisão relativa por zero");
  AE_EXPECT_TRUE(near(eval("L(0,10)"), 0), "um objeto recebe o começo da faixa");
  context.count = 3;
  context.index = 1;
  AE_EXPECT_TRUE(near(eval("L(0,10)", context), 5), "o do meio de três recebe o meio");
  context.index = 2;
  AE_EXPECT_TRUE(near(eval("L(0,10)", context), 10), "o último recebe o fim");
  AE_EXPECT_TRUE(near(eval("cos(L(0,2*pi))*5"), 5), "aninhado, como no exemplo da Unity");
  NumericExpressionContext random;
  random.seed = 42;
  const double first = eval("R(2,4)", random);
  AE_EXPECT_TRUE(first >= 2 && first < 4, "R dentro da faixa");
  AE_EXPECT_TRUE(near(eval("R(2,4)", random), first), "mesma semente, mesmo sorteio");
  random.index = 1;
  AE_EXPECT_TRUE(!near(eval("R(2,4)", random), first), "cada objeto sorteia o seu");
  AE_EXPECT_TRUE(refused("L(1)") && refused("R(1,2"), "L e R pedem dois valores fechados");
}
