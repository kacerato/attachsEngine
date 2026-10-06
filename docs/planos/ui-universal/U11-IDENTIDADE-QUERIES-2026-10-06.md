# U11 — identidade da forma nas consultas de runtime

## Problema e contrato

Dois objetos filhos podem ter Collider nº 1 e contribuir para um único Body. Retornar somente Body + UID local confunde a forma atingida com outra instância. O contrato deve preservar três informações: objeto do Body, objeto autoral do Collider e UID local do Collider. O índice interno de subforma do Jolt não é identidade persistente de autoria.

Referências oficiais consultadas: Unity **6000.0**, [RaycastHit.collider](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/RaycastHit-collider.html), [RaycastHit.rigidbody](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/RaycastHit-rigidbody.html) e [código oficial de RaycastHit](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Modules/Physics/ScriptBindings/RaycastHit.bindings.cs); Godot **4.5**, [PhysicsDirectSpaceState3D](https://docs.godotengine.org/en/4.5/classes/class_physicsdirectspacestate3d.html). A Unity distingue o Collider atingido do Rigidbody vinculado; a Godot entrega identidade do objeto e da forma nas consultas. Adaptamos esse princípio ao UID persistente local da AttachsEngine, sem copiar aparência ou hierarquia dessas engines.

## Caminho vertical

1. Manter o modelo existente de proprietário explícito do Collider e o serializer nativo; nenhum novo formato de cena é necessário.
2. Guardar `(objeto autoral, UID)` para cada parte, na mesma ordem enviada ao compound Jolt. A reconstrução precisa renovar o mapeamento quando partes são removidas/desligadas.
3. Resolver o par no caminho compartilhado de RayCast, RayCastAll, ShapeCast e Overlap. Preservar o objeto do Body e a semântica de Ignore que filtra o corpo inteiro.
4. Transportar a identidade na fronteira C++/C# e rejeitar contrato incompatível antes de acessar callbacks.
5. Expor `RayHit.BodyObject`, `ColliderObject` e `Collider` tipado, sem substituir a assinatura posicional existente. Resolver o componente exato somente quando solicitado, respeitando mundo, geração, remoção e tipo.
6. Diferenciar argumentos inválidos/recusa de um resultado sem acerto. A fachada C# entrega WorldException com o WorldStatus nativo, incluindo mundo encerrado. Não escrever parcialmente no buffer em erro; respeitar capacidade e total/truncamento.
7. Compilar cenário com dois filhos de UID igual pelo compilador real do projeto; executar consultas no Play e ler propriedades dos Colliders atingidos. Não criar painel ou controle novo para demonstrar uma API.

## Compatibilidade

**ABI44** acrescenta `colliderObject` ao final de `ScriptQueryHit`/`RawQueryHit`: tamanho **64**, offset **56**. Os slots de callbacks permanecem nas posições atuais. Os callbacks de consultas precisam negociar a versão corrente; consumidores nativos/gerenciados ABI43 precisam ser recompilados. O runtime gerenciado rejeita versão ou tamanho diferentes antes de ler ponteiros. A assinatura posicional de `RayHit` e `Object` como Body foram preservadas. Identidade ausente continua ausente; não se presume um Collider no Body.

O pacote também é usado pelo solver 2D. Seu produtor deve preencher o novo campo com o objeto real da forma 2D, mantendo o layout único. `RayHit.Collider` é a fachada de Collider **3D**; não promete reinterpretar Collider2D.

## Aceite previsto

Arquivo nativo → Body sem Collider próprio + dois filhos com UID 1 → consultas Jolt identificam os dois filhos → buffers de um e dois elementos preservam total/stride → shape cast e overlap identificam as partes → desativar uma forma e reconstruir renumera apenas índices internos → restaurar arquivo renova o mapeamento original → parar Play invalida consultas. No SDK: resolver gerações distintas, acessar o componente exato, expirar após remoção/destruição, rejeitar ABI43 e compilar o exemplo real.

O Android deve executar `CompoundQueryProbe` no Play e produzir `QUERY_IDENTITY PASS`, acompanhado de hash do APK instalado e logs da execução. Instalação ou compilação do script, isoladamente, não são esse aceite.

## Estado

**Implementado e aceito nesta cadeia delimitada.** Build host e Android concluídos; **89/89 cenários nativos de UI/autoria/runtime**, incluindo quatro contratos de consultas e o contrato de script 2D; **4/4 cenários C#**, incluindo compilação do exemplo real. O primeiro conjunto ampliado expôs dependência do motor dinâmico de uma normal em raio iniciado dentro do piso. A normal não é inventada: esse consumidor repete a consulta acima da penetração, mantendo o limite inferior configurado e obtendo uma superfície real. Os **5/5 cenários do motor** e depois os **89/89** foram repetidos com a correção. A rotação de shape query é validada/normalizada antes de chegar ao Jolt.

Android Xiaomi **25053PC47G/onyx**, APK Release/RelWithDebInfo SHA-256 **6b258df339a9da142b3e5804c1932b6efe8a188125d4915627a8b90a4ca6b6cc**, igual ao `base.apk` instalado. Projeto **API Query Owners v1** gerado pelo serializer nativo; arquivo físico igual ao original SHA **987ef171802a5fcaceebfea34692def2f84126d2715f23d186acacaf84c29874**. O SDK compilou o script no aparelho; após instalar a revisão final e reabrir a frio, o cache de assets do runtime gerenciado foi reutilizado e o Play executou a geração publicada do script, registrando sete leituras de Collider tipado: Body 2, objetos autorais 4/5, ambos UID 1, RayCast/RayCastAll/ShapeCast/Overlap. O log e a captura real mostram `QUERY_IDENTITY PASS`. Este cenário físico valida as queries, não repete o aceite físico de apoio/salto do motor.

Evidência final: [acceptance.json](../../validacao/ui-universal-2026-10-06/query-identity/acceptance.json), [log de execução](../../validacao/ui-universal-2026-10-06/query-identity/runtime.log) e [Play no aparelho](../../validacao/ui-universal-2026-10-06/query-identity/device-play-final.png). As capturas `device-setup*` são da revisão intermediária e não são aceite do APK final. **268 fontes protegidas, zero alterações**; isso não é medição de FPS/thermal nem afirma que o runtime inteiro ficou intocado. A mudança no consumidor de apoio está registrada acima. Nenhum vídeo novo foi produzido/analisado nesta etapa de API.

U11 e o roadmap universal continuam parciais: alças de edição no viewport, oclusão da cena, distribuição/limites avançados, centro de massa/apoio, skin, regeneração com overrides e perfil de custo em escala não são fechados por esta expansão. Contatos de colisão não ganharam identidade de subforma nesta entrega. O roadmap original foi preservado byte a byte e continua com 140 requisitos planejados, 20 parciais e 4 completos; a execução desta extensão é rastreada separadamente. Alterações locais, sem commit/push nesta continuação.

Continuação posterior: a pendência de seleção com oclusão geométrica foi fechada no [bloco U11 — oclusão](U11-OCLUSAO-2026-10-06.md), com registros próprios de host e aparelho. Os resultados desta página continuam atribuídos à revisão de queries; as demais pendências não foram encerradas pelo novo bloco.
