# Montagem pronta, produzida no Android

Cena e recursos foram salvos pela attachsEngine no Xiaomi 25053PC47G, com o APK Release `57dc28f0ccd363172913d11d1910c6b6a47500773405b0e79050f9a20fa5f2a2`. O arquivo de cena reaberto manteve SHA-256 `e7e6b94906ef0cc61fbfa53d3507775a310a8033cc27fca5cb91cfdf13813cd5`.

Copie toda a pasta, inclusive `.astra`, para `Projetos` do editor e abra **UI Hierarchy Sources Ready**. O nome de exibição foi alterado no descriptor; cena, GLB, registro e mapas de identidade são os arquivos produzidos no aparelho. Caches, lock de gravação e assemblies compilados foram omitidos; fontes continuam presentes para recompilação.

`CompoundPlayer`: Body + DynamicBodyMotor + 16 Colliders Mesh convexos com recursos próprios. Sem renderer substituto. Visuais continuam em `ScaledBranch/RotatedPart` e `SecondInstance`; câmera e Canvas apontam ao corpo principal. Em Play, joystick move, **SALTAR** salta e **IMPULSO** aplica impulso.

O GLB em `Collision` conserva GUIDs, IDs e matrizes das fontes, intervalos de triângulos e objeto de origem de cada casco. Isso documenta a revisão; não é uma receita de regeneração automática. Para começar a autoria do zero, use `../HierarchySources`.
