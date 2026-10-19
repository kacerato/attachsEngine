# Verifica que 9 buscas PCF bilineares espaçadas de 1 texel == 4 textureGather
# com pesos (1-f, 1, 1, f), como em environment_lighting.glsl.
import math, random
random.seed(7)
N=64
depth=[[random.random() for _ in range(N)] for _ in range(N)]
def cmp(i,j,z): return 1.0 if z<=depth[j][i] else 0.0
def bilinear_pcf(u,v,z):
    t=u*N-0.5; s=v*N-0.5; i0=math.floor(t); j0=math.floor(s); fx=t-i0; fy=s-j0
    return (cmp(i0,j0,z)*(1-fx)*(1-fy)+cmp(i0+1,j0,z)*fx*(1-fy)+cmp(i0,j0+1,z)*(1-fx)*fy+cmp(i0+1,j0+1,z)*fx*fy)
def reference(u,v,z):
    return sum(bilinear_pcf(u+x/N,v+y/N,z) for y in (-1,0,1) for x in (-1,0,1))/9
def gather(u,v,z):
    t=u*N-0.5; s=v*N-0.5; i0=math.floor(t); j0=math.floor(s)
    return (cmp(i0,j0+1,z),cmp(i0+1,j0+1,z),cmp(i0+1,j0,z),cmp(i0,j0,z))  # x,y,z,w
def candidate(u,v,z):
    tx=u*N-0.5; ty=v*N-0.5; bx=math.floor(tx); by=math.floor(ty); fx=tx-bx; fy=ty-by
    g00=gather(bx/N,by/N,z); g10=gather((bx+2)/N,by/N,z); g01=gather(bx/N,(by+2)/N,z); g11=gather((bx+2)/N,(by+2)/N,z)
    lx=1-fx; ly=1-fy; X,Y,Z,W=0,1,2,3
    return (g00[W]*lx*ly+g00[Z]*ly+g00[X]*lx+g00[Y]+g10[W]*ly+g10[Z]*fx*ly+g10[X]+g10[Y]*fx+
            g01[W]*lx+g01[Z]+g01[X]*lx*fy+g01[Y]*fy+g11[W]+g11[Z]*fx+g11[X]*fy+g11[Y]*fx*fy)/9
worst=max(abs(reference(u,v,z)-candidate(u,v,z)) for u,v,z in
          ((random.uniform(4/N,1-4/N),random.uniform(4/N,1-4/N),random.random()) for _ in range(20000)))
print('diferença máxima', worst)
