// Grade local de ondulação dinâmica, escrita pelo runtime e lida tanto pelo
// caminho analítico quanto pelo espectral. Ela não pertence ao include FFT:
// interação de casco é uma camada ortogonal ao provedor do oceano.
layout(std430,set=1,binding=15) readonly buffer WaterRipples { float rippleHeights[]; };

float readRippleHeight(int column,int row,int resolution) {
  if(column<0||row<0||column>=resolution||row>=resolution) return 0.0;
  return rippleHeights[row*resolution+column];
}

// Bilinear idêntica à referência CPU. A moldura virtual fora da grade vale
// zero e os dois texels externos recebem um fade cúbico: altura e derivada
// chegam a zero sem desenhar o retângulo da simulação sobre o oceano.
float sampleWaterRipple(vec2 world) {
  int resolution=int(environment.waterRippleArea.w+0.5);
  if(resolution<2) return 0.0;
  float area=environment.waterRippleArea.z;
  if(area<=0.0) return 0.0;
  float cell=area/float(resolution);
  vec2 relative=world-environment.waterRippleArea.xy;
  float edgeDistance=min(area*0.5-abs(relative.x),area*0.5-abs(relative.y));
  if(edgeDistance<=0.0) return 0.0;
  vec2 local=(relative+vec2(area*0.5))/cell-vec2(0.5);
  ivec2 base=ivec2(floor(local));
  vec2 fraction=fract(local);
  float top=mix(readRippleHeight(base.x,base.y,resolution),
                readRippleHeight(base.x+1,base.y,resolution),fraction.x);
  float bottom=mix(readRippleHeight(base.x,base.y+1,resolution),
                   readRippleHeight(base.x+1,base.y+1,resolution),fraction.x);
  float edgeFade=smoothstep(0.0,2.0*cell,edgeDistance);
  return mix(top,bottom,fraction.y)*edgeFade;
}

// Inclinação por diferença central. O passo é argumento porque cada estágio
// tem uma escala útil diferente: o vértice não ganha detalhe abaixo da malha e
// o fragmento não ganha detalhe abaixo do footprint do pixel.
vec2 sampleWaterRippleSlope(vec2 world,float step) {
  float inverse=0.5/max(step,1.0e-4);
  return vec2(sampleWaterRipple(world+vec2(step,0.0))-
              sampleWaterRipple(world-vec2(step,0.0)),
              sampleWaterRipple(world+vec2(0.0,step))-
              sampleWaterRipple(world-vec2(0.0,step)))*inverse;
}
