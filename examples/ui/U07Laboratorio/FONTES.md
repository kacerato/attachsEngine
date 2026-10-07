# Ambiente U07

Humanoide de teste Godot TPS Demo: 145 juntas, oito clipes, CC-BY-3.0, Juan Linietsky e Fernando Miguel Calabró. Fonte https://github.com/godotengine/tps-demo/blob/master/player/model/player.glb ; derivação reproduzível em tools/prepare-u07-motion-assets.py. Materiais de cor explícitos, sem declarar as texturas externas do Godot como importadas. Fox: PixelMannen/tomkranis/AsoboStudio/scurest, licenças em Sources.

Piso concreto PBR: Poly Haven, CC0, https://polyhaven.com/a/concrete_floor_02 .

MotorAnimationDriver usa velocidade e apoio reais, mistura e fase de passada; não é root motion, retargeting nem IK. Colisão medida na malha autoral vinculada ao Body, não por osso a cada quadro.
