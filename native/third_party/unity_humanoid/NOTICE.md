# Conversão Humanoid da Unity (unity_humanoid.cpp)

`native/resources/unity_humanoid.cpp` traduz para C++ as tabelas e a matemática
de músculos de `humanoid/human_trait.gd` e `humanoid/transform_util.gd` do
[Unidot Importer](https://github.com/V-Sekai/unidot_importer):

- Copyright 2023-present Lyuma and contributors
- Copyright 2022-2023 lox9973 (reconstrução do HumanPoseHandler no ShaderMotion)

Licença: Apache License 2.0 (`LICENSE-APACHE-2.0.txt` nesta pasta). Mudanças
na tradução: tabelas em C++, cálculo em double, redirecionamento para o
esqueleto do personagem pela pose em T e trilhas no formato de clipe da Astra.

As rotações de descanso do esqueleto de referência vêm de
`scene/resources/skeleton_profile.cpp` (SkeletonProfileHumanoid) da
[Godot Engine 4.3](https://github.com/godotengine/godot), licença MIT,
Copyright (c) 2014-present Godot Engine contributors; Copyright (c) 2007-2014
Juan Linietsky, Ariel Manzur.
