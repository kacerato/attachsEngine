#include "harness.h"
#include "editor/editor_numeric_expression.h"
#include "editor/editor_archive.h"
#include "editor/editor_history.h"
#include "scene/script_behavior.h"

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

// Unity AnimationCurve.Evaluate e modos de tangente: Hermite entre chaves,
// Linear vira reta, Constant segura, ClampedAuto não passa dos vizinhos,
// Clamp/Loop/PingPong fora das chaves; presets de fábrica são válidos.
AE_TEST(script_curve_tangents_wrap_and_evaluation) {
  scene::ScriptCurve curve;
  AE_EXPECT_TRUE(scene::parseScriptCurve("0 0 2 0 0 1 1 2 2 0 1 1 1 1 2 2 0", curve), "linear lida");
  AE_EXPECT_TRUE(near(scene::evaluateScriptCurve(curve, .25f), .25), "Linear é reta");
  AE_EXPECT_TRUE(near(scene::evaluateScriptCurve(curve, -3.f), 0) && near(scene::evaluateScriptCurve(curve, 4.f), 1), "Clamp nas pontas");
  curve.post = scene::CurveWrapMode::Loop;
  AE_EXPECT_TRUE(std::abs(scene::evaluateScriptCurve(curve, 1.25f) - .25f) < 1e-5f, "Loop repete");
  curve.post = scene::CurveWrapMode::PingPong;
  AE_EXPECT_TRUE(std::abs(scene::evaluateScriptCurve(curve, 1.25f) - .75f) < 1e-5f, "PingPong volta");
  curve.keys[0].right = scene::CurveTangentMode::Constant;
  AE_EXPECT_TRUE(near(scene::evaluateScriptCurve(curve, .9f), 0), "Constant segura o valor até a próxima chave");
  scene::ScriptCurve peak;
  peak.keys = {{0, 0}, {.5f, 1}, {1, 0}};
  peak.updateTangents();
  AE_EXPECT_TRUE(near(peak.keys[1].in, 0) && near(peak.keys[1].out, 0), "ClampedAuto deixa plano o extremo local");
  float highest = 0;
  for (u32 i = 0; i <= 100; ++i) highest = std::max(highest, scene::evaluateScriptCurve(peak, i / 100.f));
  AE_EXPECT_TRUE(highest <= 1.0001f, "ClampedAuto não passa do pico");
  for (const auto &preset : scene::curveFactoryPresets)
    AE_EXPECT_TRUE(scene::validScriptPropertyValue("curve", preset.value), preset.name);
  AE_EXPECT_TRUE(!scene::validScriptPropertyValue("curve", "0 0 2 1 0 0 0 0 0 0 1 1 0 0 0 0 0"), "tempos repetidos recusados");
  AE_EXPECT_TRUE(scene::validScriptPropertyValue("curve", "0 0 0"), "curva vazia é válida");
}
AE_TEST(script_curve_runtime_edits_precision_extremes_and_invalid_input_match_sdk) {
  scene::ScriptCurve curve;curve.keys={{0,0,0,0,scene::CurveTangentMode::Linear,scene::CurveTangentMode::Linear},
                                    {1,2,0,0,scene::CurveTangentMode::Linear,scene::CurveTangentMode::Linear}};
  curve.keys[1].value=4;
  AE_EXPECT_TRUE(near(scene::evaluateScriptCurve(curve,.25f),1),"Linear derives current value without authoring-cache refresh");
  curve.keys[0].right=curve.keys[1].left=scene::CurveTangentMode::Auto;
  AE_EXPECT_TRUE(near(scene::evaluateScriptCurve(curve,.25f),.625),"Auto endpoints flatten");
  scene::ScriptCurve close;close.keys={{1,1},{std::nextafter(1.f,2.f),2}};const auto encoded=scene::scriptCurveValue(close);
  scene::ScriptCurve reopened;AE_EXPECT_TRUE(scene::parseScriptCurve(encoded,reopened) && reopened.keys[1].time==close.keys[1].time,"float identity survives archive without collapsing adjacent times");
  const float maximum=std::numeric_limits<float>::max();curve.keys={{-maximum,-maximum,0,0,scene::CurveTangentMode::Linear,scene::CurveTangentMode::Linear},
                                                                 {maximum,maximum,0,0,scene::CurveTangentMode::Linear,scene::CurveTangentMode::Linear}};
  curve.updateTangents();AE_EXPECT_TRUE(curve.valid() && near(scene::evaluateScriptCurve(curve,0),0),"double intermediates avoid overflowing a finite interval");
  float result=17;curve.pre=static_cast<scene::CurveWrapMode>(99);
  AE_EXPECT_TRUE(!scene::tryEvaluateScriptCurve(curve,0,result) && result==17,"invalid mode is reported before publishing a sample");
  curve.pre=scene::CurveWrapMode::Clamp;
  AE_EXPECT_TRUE(!scene::tryEvaluateScriptCurve(curve,std::numeric_limits<float>::infinity(),result) && result==17,"non-finite sample refused");
  scene::ScriptGradient gradient;float rgba[4]{17,17,17,17};gradient.mode=static_cast<scene::GradientMode>(99);
  AE_EXPECT_TRUE(!scene::tryEvaluateScriptGradient(gradient,.5f,rgba) && rgba[0]==17,"gradient validates mode before touching output");
  EditorDocument document;const auto object=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Curve consumer");
  auto value=*document.find(object);auto *script=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="family.Curves";script->source="Scripts/Curves.cs";
  gradient.mode=scene::GradientMode::Perceptual;
  script->properties={{"height","curve",encoded},{"colors","gradient",scene::scriptGradientValue(gradient)}};
  EditorHistory history;AE_EXPECT_TRUE(history.applyValues(document,object,value),"author fields atomically");
  const auto archive=serializeEditorDocument(document,7);EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,7,restored) && serializeEditorDocument(restored,7)==archive,"curve and gradient field payloads roundtrip in real scene archive");
  AE_EXPECT_TRUE(history.undo(document) && !document.find(object)->components.find(scene::ScriptBehavior::descriptor),"Undo removes the complete field publication");
  AE_EXPECT_TRUE(history.redo(document) && serializeEditorDocument(document,7)==archive,"Redo restores exact field values");
}

// Unity ScriptReference/Gradient.Evaluate: fora das paradas vale a ponta;
// Blend interpola, Fixed dá degrau na parada seguinte, Perceptual passa por
// Oklab (de preto a branco o meio perceptual é mais escuro que o linear).
AE_TEST(script_gradient_format_validation_and_evaluation) {
  scene::ScriptGradient gradient;
  AE_EXPECT_TRUE(scene::parseScriptGradient("0 2 0.25 1 0 0 0.75 0 1 0 2 0 1 1 0", gradient), "formato lido");
  float rgba[4];
  scene::evaluateScriptGradient(gradient, 0.f, rgba);
  AE_EXPECT_TRUE(near(rgba[0], 1) && near(rgba[1], 0) && near(rgba[3], 1), "antes da primeira parada vale a primeira");
  scene::evaluateScriptGradient(gradient, .5f, rgba);
  AE_EXPECT_TRUE(near(rgba[0], .5) && near(rgba[1], .5) && near(rgba[3], .5), "Blend no meio");
  gradient.mode = scene::GradientMode::Fixed;
  scene::evaluateScriptGradient(gradient, .5f, rgba);
  AE_EXPECT_TRUE(near(rgba[0], 0) && near(rgba[1], 1) && near(rgba[3], 0), "Fixed vale a parada seguinte");
  scene::ScriptGradient gray;
  AE_EXPECT_TRUE(scene::parseScriptGradient("2 2 0 0 0 0 1 1 1 1 2 0 1 1 0", gray), "preto a branco");
  scene::evaluateScriptGradient(gray, .5f, rgba);
  AE_EXPECT_TRUE(std::abs(rgba[0] - .125f) < .01f && std::abs(rgba[0] - rgba[2]) < 1e-4f,
                 "Perceptual: L de Oklab no meio (0,5) é 0,125 linear, não 0,5");
  AE_EXPECT_TRUE(std::abs(rgba[3] - .5f) < 1e-6f, "o alfa não passa por Oklab");
  AE_EXPECT_TRUE(scene::validScriptPropertyValue("gradient", scene::scriptGradientValue(gradient)), "formato gravado é válido");
  AE_EXPECT_TRUE(!scene::validScriptPropertyValue("gradient", "0 1 0 2 0 0 1 0 1") &&
                 scene::validScriptPropertyValue("gradient:hdr", "0 1 0 2 0 0 1 0 1"), "cor acima de 1 só em HDR");
  AE_EXPECT_TRUE(!scene::validScriptPropertyValue("gradient", "0 0 1 0 1") &&
                 !scene::validScriptPropertyValue("gradient", "0 1 1.5 1 1 1 1 0 1"), "sem parada ou fora de [0,1] recusado");
}

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
