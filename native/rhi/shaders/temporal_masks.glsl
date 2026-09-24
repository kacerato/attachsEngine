// Máscaras temporais R8 (G6-B), escritas como anexos 1 e 2 do passe
// principal com blend MAX. Só pipelines com blend (transparentes e água)
// habilitam a escrita; opacos, cobertura, céu e grade as deixam em zero.
//
// Reatividade: quanto a amostra atual deve pesar sobre o histórico -- a
// orientação do FSR 2/Arm ASR para superfícies com alpha blending é escrever o
// próprio alpha, limitado a 0,9. Composição (transparency & composition):
// onde profundidade e vetor não descrevem a cor visível.
layout(location=1) out mediump float outReactive;
layout(location=2) out mediump float outComposition;

void aetherWriteTemporalMasks(mediump float reactive, mediump float composition) {
  outReactive=clamp(reactive,0.0,0.9);
  outComposition=clamp(composition,0.0,1.0);
}

// Material com blend: a cobertura do alpha é a reatividade e a composição.
void aetherBlendedTemporalMasks(mediump float alpha) {
  aetherWriteTemporalMasks(alpha,alpha);
}

// Água: a superfície se move (ondas, reflexo, refração) sem escrever vetor
// nem profundidade própria. A composição cobre toda a água visível; a
// reatividade é moderada no corpo e forte na espuma, que muda de quadro a
// quadro sem correspondência reprojetável.
void aetherWaterTemporalMasks(mediump float coverage, mediump float foam) {
  aetherWriteTemporalMasks(0.3*coverage+0.6*foam,coverage);
}
