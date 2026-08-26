namespace Aether.Tests;

public static class MathTests
{
    [Test] public static void Cross_SegueRegraDaMaoEsquerda()
    {
        // Convenção do Aether: mão-esquerda, X direita, Y cima, Z frente.
        // Right x Up deve dar Forward.
        Assert.Close(float3.Forward, math.Cross(float3.Right, float3.Up), what: "X cross Y = Z");
        Assert.Close(float3.Right, math.Cross(float3.Up, float3.Forward), what: "Y cross Z = X");
        Assert.Close(float3.Up, math.Cross(float3.Forward, float3.Right), what: "Z cross X = Y");
    }

    [Test] public static void Normalized_NuncaProduzNaN()
    {
        Assert.Equal(float3.Zero, float3.Zero.Normalized, "vetor nulo normaliza para zero, não NaN");
        Assert.Equal(float3.Zero, new float3(1e-30f, 0f, 0f).Normalized, "vetor desprezível idem");
        Assert.Close(1f, new float3(3f, 4f, 12f).Normalized.Length);
    }

    [Test] public static void Quaternion_RotacaoDe90GrausEmY()
    {
        var q = quaternion.AxisAngle(float3.Up, 90f * math.Deg2Rad);
        // Girar "frente" 90° em torno de Y (mão-esquerda, sentido horário visto de cima) dá "direita".
        Assert.Close(float3.Right, q * float3.Forward, 1e-5f, "Z girado 90° em Y vira X");
        Assert.Close(float3.Up, q * float3.Up, 1e-5f, "o eixo de rotação não se move");
    }

    [Test] public static void Quaternion_NormalizedPreservaUnitarioECorrigeMagnitude()
    {
        var unit = quaternion.AxisAngle(float3.Up, 0.75f);
        Assert.Equal(unit, unit.Normalized, "quaternion já unitário usa o caminho exato sem sqrt");

        var scaled = new quaternion(unit.X * 3f, unit.Y * 3f, unit.Z * 3f, unit.W * 3f);
        var normalized = scaled.Normalized;
        float norm = MathF.Sqrt(normalized.X * normalized.X + normalized.Y * normalized.Y +
                                normalized.Z * normalized.Z + normalized.W * normalized.W);
        Assert.Close(1f, norm, 1e-5f, "magnitude fora da tolerância continua sendo corrigida");
    }

    [Test] public static void TransformChild_CaminhoIdentidadeMantemContratoGeral()
    {
        var parent = Transform.FromPosition(new float3(10, 2, -3));
        var childRotation = quaternion.AxisAngle(float3.Up, 0.5f);
        var child = new Transform(new float3(1, 4, 2), childRotation, new float3(2, 3, 4));

        var result = parent.TransformChild(child);
        Assert.Close(new float3(11, 6, -1), result.Position);
        Assert.Close(childRotation, result.Rotation);
        Assert.Close(child.Scale, result.Scale);
    }

    [Test] public static void Quaternion_InversoDesfazRotacao()
    {
        var q = quaternion.Euler(0.3f, -1.1f, 0.7f);
        var v = new float3(1.5f, -2f, 0.25f);
        Assert.Close(v, q.Inverse * (q * v), 1e-4f);
        Assert.Close(quaternion.Identity, q * q.Inverse, 1e-5f);
    }

    [Test] public static void Quaternion_ComposicaoEhAssociativaEOrdemImporta()
    {
        var a = quaternion.AxisAngle(float3.Up, 0.9f);
        var b = quaternion.AxisAngle(float3.Right, 0.4f);
        var v = new float3(0.3f, 0.8f, -1.2f);
        // (a*b) aplica b primeiro, depois a.
        Assert.Close(a * (b * v), (a * b) * v, 1e-4f, "composição casa com aplicação sequencial");
        Assert.True(math.Distance((a * b) * v, (b * a) * v) > 0.1f, "rotação não é comutativa: a ordem muda o resultado");
    }

    [Test] public static void Quaternion_SlerpPegaCaminhoCurto()
    {
        var a = quaternion.Identity;
        var b = quaternion.AxisAngle(float3.Up, 350f * math.Deg2Rad);
        var mid = quaternion.Slerp(a, b, 0.5f);
        // O caminho curto de 0° para 350° passa por -5°, não por +175°.
        float3 rotated = mid * float3.Forward;
        Assert.True(rotated.X < 0f, "slerp foi pelo lado curto (negativo), não pelo longo");
    }

    [Test] public static void Quaternion_LookRotationOlhaParaOAlvo()
    {
        var dir = new float3(1f, 0f, 1f).Normalized;
        var q = quaternion.LookRotation(dir, float3.Up);
        Assert.Close(dir, q * float3.Forward, 1e-4f);
    }

    [Test] public static void Matriz_TRSCasaComTransformComponente()
    {
        var t = new Transform(new float3(2f, -1f, 5f),
                              quaternion.Euler(0.2f, 0.5f, -0.3f),
                              new float3(2f, 0.5f, 1.5f));
        var p = new float3(0.4f, -0.7f, 1.1f);
        Assert.Close(t.TransformPoint(p), t.ToMatrix().TransformPoint(p), 1e-4f,
            "o caminho rápido (Transform) e o caminho geral (matriz) concordam");
    }

    [Test] public static void Matriz_InversaDesfazTransformacao()
    {
        var m = float4x4.TRS(new float3(3f, 1f, -2f), quaternion.Euler(0.7f, -0.2f, 1.3f), new float3(1.5f, 2f, 0.75f));
        var p = new float3(1f, 2f, 3f);
        Assert.Close(p, m.Inverse.TransformPoint(m.TransformPoint(p)), 1e-3f);
    }

    [Test] public static void Matriz_InversaDeSingularNaoExplode()
    {
        var singular = float4x4.Scale(new float3(1f, 0f, 1f));   // achatada: determinante zero
        Assert.Equal(float4x4.Identity, singular.Inverse, "matriz singular devolve identidade em vez de NaN");
    }

    [Test] public static void Matriz_MultiplicacaoRespeitaOrdemPaiFilho()
    {
        var pai   = float4x4.TRS(new float3(10f, 0f, 0f), quaternion.AxisAngle(float3.Up, math.PI * 0.5f), float3.One);
        var filho = float4x4.Translate(new float3(0f, 0f, 2f));
        // O filho está 2 unidades à frente do pai; o pai está girado 90° em Y, logo o filho vai para +X.
        var mundo = pai * filho;
        Assert.Close(new float3(12f, 0f, 0f), mundo.TransformPoint(float3.Zero), 1e-4f);
    }

    [Test] public static void Projecao_VulkanColocaProfundidadeEmZeroUm()
    {
        var proj = float4x4.PerspectiveVulkan(60f * math.Deg2Rad, 16f / 9f, 0.1f, 100f);

        float4 near = proj.Transform(new float4(0f, 0f, 0.1f, 1f));
        float4 far  = proj.Transform(new float4(0f, 0f, 100f, 1f));
        Assert.Close(0f, near.Z / near.W, 1e-4f, "plano perto mapeia para z=0");
        Assert.Close(1f, far.Z / far.W, 1e-4f, "plano longe mapeia para z=1");
    }

    [Test] public static void Projecao_ReverseZInverteAProfundidade()
    {
        var proj = float4x4.PerspectiveReverseZ(60f * math.Deg2Rad, 16f / 9f, 0.1f);
        float4 near = proj.Transform(new float4(0f, 0f, 0.1f, 1f));
        Assert.Close(1f, near.Z / near.W, 1e-4f, "reverse-Z: perto = 1 (máxima precisão de float perto da câmera)");
        float4 farAway = proj.Transform(new float4(0f, 0f, 10000f, 1f));
        Assert.True(farAway.Z / farAway.W < 0.001f, "reverse-Z: longe tende a 0");
    }

    [Test] public static void Projecao_YInvertidoParaConvencaoVulkan()
    {
        var proj = float4x4.PerspectiveVulkan(60f * math.Deg2Rad, 1f, 0.1f, 100f);
        float4 acima = proj.Transform(new float4(0f, 1f, 5f, 1f));
        Assert.True(acima.Y / acima.W < 0f,
            "no clip space do Vulkan o eixo Y aponta para baixo: um ponto acima da câmera tem Y negativo");
    }

    [Test] public static void LookAt_ColocaCameraNaOrigemOlhandoParaFrente()
    {
        var view = float4x4.LookAt(new float3(0f, 0f, -5f), float3.Zero, float3.Up);
        Assert.Close(float3.Zero, view.TransformPoint(new float3(0f, 0f, -5f)), 1e-4f, "a posição da câmera vira a origem");
        var alvo = view.TransformPoint(float3.Zero);
        Assert.Close(5f, alvo.Z, 1e-4f, "o alvo fica 5 unidades à frente no espaço de visão");
    }

    [Test] public static void Bounds_EncapsulateConstroiAPartirDoVazio()
    {
        var b = Bounds.Empty;
        Assert.False(b.IsValid, "bounds vazio é inválido até receber um ponto");
        b.Encapsulate(new float3(1f, 2f, 3f));
        b.Encapsulate(new float3(-1f, 0f, 5f));
        Assert.True(b.IsValid);
        Assert.Close(new float3(-1f, 0f, 3f), b.Min);
        Assert.Close(new float3(1f, 2f, 5f), b.Max);
    }

    [Test] public static void Bounds_TransformCobreOObjetoGirado()
    {
        var b = Bounds.FromCenterExtents(float3.Zero, float3.One);      // cubo unitário
        var m = float4x4.Rotate(quaternion.AxisAngle(float3.Up, 45f * math.Deg2Rad));
        var t = b.Transform(m);
        // Um cubo girado 45° precisa de um AABB maior em X e Z, mas não em Y.
        Assert.Close(MathF.Sqrt(2f), t.Extents.X, 1e-4f);
        Assert.Close(1f, t.Extents.Y, 1e-4f);
        Assert.True(t.Contains(m.TransformPoint(new float3(1f, 1f, 1f))), "o AABB contém o canto girado");
    }

    [Test] public static void Ray_AcertaEErraAABB()
    {
        var b = Bounds.FromCenterExtents(new float3(0f, 0f, 10f), float3.One);
        Assert.True(new Ray(float3.Zero, float3.Forward).Intersects(b, out float t), "raio direto acerta");
        Assert.Close(9f, t, 1e-4f, "entra na face frontal, a 9 unidades");
        Assert.False(new Ray(float3.Zero, float3.Up).Intersects(b, out _), "raio para cima erra");
        Assert.False(new Ray(float3.Zero, -float3.Forward).Intersects(b, out _), "raio para trás erra");
    }

    [Test] public static void Ray_ClosestPointOnAxis_EhABaseDoGizmoDeEixo()
    {
        // O dedo arrasta na tela; o raio da câmera é projetado no eixo X do gizmo.
        var ray = new Ray(new float3(3f, 2f, -1f), float3.Forward);
        float t = ray.ClosestPointOnAxis(float3.Zero, float3.Right);
        Assert.Close(3f, t, 1e-4f, "o ponto mais próximo no eixo X é x=3");
    }

    [Test] public static void Ray_ClosestPointOnAxis_NaoExplodeComEixoParalelo()
    {
        var ray = new Ray(float3.Zero, float3.Right);
        float t = ray.ClosestPointOnAxis(new float3(0f, 1f, 0f), float3.Right);
        Assert.Close(0f, t, 1e-4f, "raio paralelo ao eixo devolve 0 em vez de NaN");
    }

    [Test] public static void Frustum_CulaOQueEstaAtrasEForaDaVista()
    {
        var view = float4x4.LookAt(float3.Zero, float3.Forward, float3.Up);
        var proj = float4x4.PerspectiveVulkan(60f * math.Deg2Rad, 1f, 0.1f, 100f);
        var f = Frustum.FromViewProjection(proj * view);

        Assert.True(f.Intersects(Bounds.FromCenterExtents(new float3(0f, 0f, 10f), float3.One)),
            "objeto à frente é visível");
        Assert.False(f.Intersects(Bounds.FromCenterExtents(new float3(0f, 0f, -10f), float3.One)),
            "objeto atrás é culado");
        Assert.False(f.Intersects(Bounds.FromCenterExtents(new float3(0f, 0f, 500f), float3.One)),
            "objeto além do plano distante é culado");
        Assert.False(f.Intersects(Bounds.FromCenterExtents(new float3(100f, 0f, 10f), float3.One)),
            "objeto muito à direita é culado");
        Assert.True(f.Intersects(Bounds.FromCenterExtents(new float3(0f, 0f, 10f), new float3(1000f))),
            "objeto gigante que envolve a câmera não é culado");
    }

    [Test] public static void Snap_ArredondaParaAGradeDoEditor()
    {
        Assert.Close(0.5f, math.Snap(0.62f, 0.25f));
        Assert.Close(-1f, math.Snap(-0.9f, 0.5f));
        Assert.Close(3.7f, math.Snap(3.7f, 0f), what: "passo zero desliga o snap");
    }

    [Test] public static void Damp_EhIndependenteDaTaxaDeQuadros()
    {
        // O mesmo tempo total deve chegar ao mesmo lugar, com 1 passo grande ou 10 pequenos.
        const float halfLife = 0.1f, total = 0.5f;
        float umPasso = math.Damp(0f, 1f, halfLife, total);

        float muitosPassos = 0f;
        for (int i = 0; i < 10; i++) muitosPassos = math.Damp(muitosPassos, 1f, halfLife, total / 10f);

        Assert.Close(umPasso, muitosPassos, 1e-4f);
        Assert.Close(0.5f, math.Damp(0f, 1f, halfLife, halfLife), 1e-4f, "após uma meia-vida, metade do caminho");
    }

    [Test] public static void AlignUp_ELimitesDeAlocador()
    {
        Assert.Equal(16, math.AlignUp(1, 16));
        Assert.Equal(16, math.AlignUp(16, 16));
        Assert.Equal(32, math.AlignUp(17, 16));
        Assert.Equal(0, math.AlignUp(0, 64));
        Assert.Equal(256, math.NextPowerOfTwo(200));
        Assert.Equal(1, math.NextPowerOfTwo(0));
        Assert.Equal(1024, math.NextPowerOfTwo(1024));
    }

    [Test] public static void Math_NaoAlocaNoCaminhoQuente()
    {
        // Materializa o KPI "zero alocação de GC por frame" para a camada matemática.
        var t = new Transform(new float3(1f, 2f, 3f), quaternion.Euler(0.1f, 0.2f, 0.3f), float3.One);
        var b = Bounds.FromCenterExtents(float3.Zero, float3.One);
        var proj = float4x4.PerspectiveVulkan(1f, 1.7f, 0.1f, 100f);
        float acc = 0f;

        Assert.NoAlloc(() =>
        {
            for (int i = 0; i < 1000; i++)
            {
                var m = t.ToMatrix() * proj;
                var f = Frustum.FromViewProjection(m);
                if (f.Intersects(b)) acc += m.C3.X;
                acc += t.TransformPoint(float3.One).X;
                acc += new Ray(float3.Zero, float3.Forward).ClosestPointOnAxis(float3.Zero, float3.Right);
            }
        }, "1000 iterações de transform + frustum + ray");

        Assert.True(!float.IsNaN(acc));
    }
}
