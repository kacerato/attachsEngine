using System.Numerics;
using System.Text.Json;
using Astra;
using Astra.Compilation;
using Astra.Components;
using Astra.Runtime;

namespace Aether.Tests;

// G7: a API gráfica com nomes da Unity (Astra.Rendering.Volume, VolumeProfile,
// overrides, RenderSettings, aliases de Light/Camera/MeshRenderer/
// ReflectionProbe, TimeOfDay.Speed, Material.SetFloat/SetColor) pelo caminho
// real de um script compilado: cada chamada vira a leitura/escrita por id ou o
// método de componente que o nativo implementa (tests/native/test_g7_graphics_api.cpp).
public static class G7GraphicsApiTests
{
    // Mundo: 1 raiz; 2 Volume (20); 3 malha (30) + câmera (31); 4 Hora do dia (40)
    // + luz (41) + sonda (42); 5 Configurações de iluminação (50).
    private sealed class Scene : ISceneAccess, IComponentOperationAccess
    {
        public uint WorldId => 9;
        public uint GenerationOf(ulong id) => Exists(id) ? 1u : 0u;
        public bool Exists(ulong id) => id is >= 1 and <= 5;
        public int GetActive(ulong id) => Exists(id) ? 1 : -1;
        public int GetActiveSelf(ulong id) => GetActive(id);
        public WorldStatus LastStatus => WorldStatus.Ok;
        public ulong ParentOf(ulong id) => id is >= 2 and <= 5 ? 1ul : 0ul;
        public int ChildCount(ulong id) => id == 1 ? 4 : 0;
        public ulong ChildAt(ulong id, uint index) => id == 1 && index < 4 ? 2ul + index : 0ul;
        public TransformValue GetTransform(ulong id) => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
        public bool SetTransform(ulong id, TransformValue value) => true;
        public bool SetBodyVelocity(ulong id, Vector3 value) => true;
        public bool MoveKinematic(ulong id, Vector3 value, Quaternion rotation) => true;
        public readonly List<string> Logs = [];
        public void Log(ulong id, string message) => Logs.Add(message);

        private static readonly (ulong Object, ulong Instance, string Type)[] Table =
        [
            (2, 20, "astra.render.environment"), (3, 30, "astra.render.mesh"), (3, 31, "astra.camera"),
            (4, 40, "astra.render.time_of_day"), (4, 41, "astra.render.light"), (4, 42, "astra.render.reflection_probe"),
            (5, 50, "astra.render.lighting_settings"),
        ];
        public int ComponentCount(ulong id) => Table.Count(c => c.Object == id);
        public (ulong Instance, string TypeId) ComponentAt(ulong id, uint index)
        {
            var row = Table.Where(c => c.Object == id).ElementAtOrDefault((int)index);
            return (row.Instance, row.Type ?? "");
        }
        public ulong FindComponent(ulong id, string type, uint ordinal) =>
            Table.Where(c => c.Object == id && c.Type == type).Select(c => c.Instance).ElementAtOrDefault((int)ordinal);

        // Propriedades por (instância, id). Booleanos e enums declaram o tipo.
        public readonly Dictionary<(ulong, string), (uint Kind, ulong Bits)> Values = [];
        private static uint KindOf(string id) =>
            id.StartsWith("override_") || id is "enabled" or "fog" or "post" or "hdr" or "physical_camera" or "post_processing" ? 1u :
            id is "volume_shape" or "background_type" or "kind" or "shadow_mode" or "cast_shadows" or "tone_mapper" or "dof_mode"
                or "ambient_source" or "fog_mode" or "type" or "refresh_mode" or "projection" or "rendering_layers" ? 2u :
            id is "sun_source" ? 3u : 0u;
        public bool TryGetProperty(ulong o, ulong i, string id, out uint kind, out ulong bits)
        {
            if (Values.TryGetValue((i, id), out var v)) { kind = v.Kind; bits = v.Bits; return true; }
            kind = KindOf(id); bits = 0; return true;
        }
        public bool SetProperty(ulong o, ulong i, string id, uint kind, ulong bits) { Values[(i, id)] = (kind, bits); Writes.Add(id); return true; }
        public bool SetTriple(ulong o, ulong i, string id, Vector3 v)
        {
            foreach (var (suffix, value) in new[] { (".r", v.X), (".g", v.Y), (".b", v.Z), (".x", v.X), (".y", v.Y), (".z", v.Z) })
                Values[(i, id + suffix)] = (0, BitConverter.SingleToUInt32Bits(value));
            Writes.Add(id); return true;
        }
        public readonly Dictionary<(ulong, string, uint), (uint Kind, ulong Bits)> Slots = [];
        public bool TryGetSlotProperty(ulong o, ulong i, string id, uint slot, out uint kind, out ulong bits)
        {
            if (Slots.TryGetValue((i, id, slot), out var v)) { kind = v.Kind; bits = v.Bits; return true; }
            kind = id.StartsWith("material.override") ? 2u : 0u; bits = 0; return true;
        }
        public bool SetSlotProperty(ulong o, ulong i, string id, uint slot, uint kind, ulong bits) { Slots[(i, id, slot)] = (kind, bits); Writes.Add(id); return true; }
        public readonly Dictionary<(ulong, string), AssetGuid> Resources = [];
        public bool TryGetResource(ulong o, ulong i, string id, uint slot, out AssetGuid value) => Resources.TryGetValue((i, id), out value) || true;
        public bool SetResource(ulong o, ulong i, string id, uint slot, AssetGuid value) { Resources[(i, id)] = value; Writes.Add(id); return true; }
        public readonly List<string> Writes = [];

        // Métodos de componente: o Volume guarda as caixas e a cópia do perfil.
        public bool Instantiated;
        public readonly HashSet<long> Overridden = [];
        public readonly HashSet<long> Inactive = [];
        public readonly List<string> Calls = [];
        public bool InvokeComponentMethod(ulong objectId, uint world, uint generation, ulong instanceId, string method,
            ReadOnlySpan<ComponentValue> arguments, out ComponentValue result)
        {
            Calls.Add(method + (arguments.Length > 0 ? "(" + string.Join(",", arguments.ToArray().Select(a => a.ToString())) + ")" : ""));
            result = default;
            switch (method)
            {
                case "instantiate_profile": Instantiated = true; break;
                case "has_instantiated_profile": result = ComponentValue.Boolean(Instantiated); break;
                case "parameter_overridden": result = ComponentValue.Boolean(Overridden.Contains(arguments[0].AsInteger())); break;
                case "set_parameter_overridden":
                    if (arguments[1].AsBoolean()) Overridden.Add(arguments[0].AsInteger()); else Overridden.Remove(arguments[0].AsInteger());
                    break;
                case "component_active": result = ComponentValue.Boolean(!Inactive.Contains(arguments[0].AsInteger())); break;
                case "set_component_active":
                    if (arguments[1].AsBoolean()) Inactive.Remove(arguments[0].AsInteger()); else Inactive.Add(arguments[0].AsInteger());
                    break;
            }
            return true;
        }
        public int PollComponentEvents(Span<ComponentEventRecord> destination) => 0;
        public bool DeclaresComponentEvent(string typeId, string eventId) => true;
    }

    private static Scene Run(string body)
    {
        var root = Path.Combine(Path.GetTempPath(), "astra-g7-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            File.WriteAllText(Path.Combine(root, "Probe.cs"), """
                using System.Numerics;
                using Astra;
                using Astra.Components;
                using Astra.Rendering;
                using Color = Astra.Color;
                [ComponentId("test.g7")]
                public sealed class Probe : Behavior
                {
                    void Say(object value) => Scene.Log(ObjectId, value?.ToString() ?? "null");
                    public override void Start()
                    {
                """ + body + """
                    }
                }
                """);
            var result = new ProjectCompiler().Build(root);
            Assert.True(result.Success, string.Join("\n", result.Diagnostics.Select(d => d.Message)));
            using var world = new BehaviorWorld();
            var scene = new Scene();
            // Os números do Log saem na cultura invariante, como no aparelho.
            var culture = System.Globalization.CultureInfo.CurrentCulture;
            System.Globalization.CultureInfo.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;
            try { world.Start(result.Project!, scene, [new(2, 90, "test.g7", true, new Dictionary<string, JsonElement>())]); }
            finally { System.Globalization.CultureInfo.CurrentCulture = culture; }
            Assert.Equal(0, world.Failures.Count, string.Join("\n", world.Failures.Select(f => f.Message)));
            return scene;
        }
        finally { Directory.Delete(root, true); }
    }

    [Test]
    public static void ParameterTable_IsGeneratedFromTheRecordedVolumeTable()
    {
        Assert.Equal(VolumeParameterTable.Ids.Length, VolumeParameterTable.Groups.Length);
        Assert.Equal(VolumeParameterTable.Ids.Length, VolumeParameterTable.Aliases.Length);
        Assert.Equal(19, VolumeParameterTable.GroupAddedProperties.Length, "19 overrides como renderer::VolumeGroup");
        Assert.Equal("override_bloom", VolumeParameterTable.GroupAddedProperties[8]);
        var bloom = VolumeParameterTable.IndexOf("bloom_intensity");
        Assert.True(bloom > 0 && VolumeParameterTable.Groups[bloom] == 8, "Bloom › Intensity no grupo 8");
        Assert.Equal(VolumeParameterTable.IndexOf("bloom_tint"), VolumeParameterTable.IndexOf("bloom_tint.g"), "canal de trio");
        Assert.Equal(VolumeParameterTable.IndexOf("grading_lift"), VolumeParameterTable.IndexOf("grading_lift_color"), "alias do trackball");
        Assert.Equal(-1, VolumeParameterTable.IndexOf("weight"), "peso do Volume não é parâmetro do perfil");
    }

    [Test]
    public static void VolumeProfile_TryGetAddOverrideAndActiveFollowUnity()
    {
        var scene = Run("""
                var volume = Volume.Get(Object)!;
                Say(volume.HasInstantiatedProfile());
                var profile = volume.Profile;
                Say(volume.HasInstantiatedProfile());
                Say(profile.TryGet<Bloom>(out var missing));
                var bloom = profile.Add<Bloom>();
                Say(profile.Has<Bloom>());
                bloom.Intensity.Override(2.5f);
                Say(bloom.Intensity.OverrideState + ":" + bloom.Intensity.Value);
                bloom.Tint.Value = new Color(1, .5f, .25f);
                var dof = profile.Add<DepthOfField>(overrides: true);
                dof.Mode.Value = DepthOfFieldMode.Bokeh;
                dof.Active = false;
                Say(dof.Active + ":" + dof.Mode.Value);
                volume.Weight = .75f;
                profile.Remove<Bloom>();
                Say(profile.Has<Bloom>());
            """);
        Assert.Equal("False True False True True:2.5 False:Bokeh False", string.Join(' ', scene.Logs));
        Assert.True(scene.Calls.Contains("instantiate_profile"), "Volume.profile copia o perfil compartilhado");
        var intensity = VolumeParameterTable.IndexOf("bloom_intensity");
        Assert.True(scene.Overridden.Contains(intensity), "Override marca a caixa do parâmetro");
        var dofCount = VolumeParameterTable.Groups.Count(g => g == 15);
        Assert.Equal(dofCount, VolumeParameterTable.Groups.Select((g, i) => (g, i)).Count(p => p.g == 15 && scene.Overridden.Contains(p.i)),
                     "Add(overrides: true) marca todas as caixas do override");
        Assert.True(scene.Inactive.Contains(15), "VolumeComponent.active desliga o cabeçalho");
        Assert.Equal(BitConverter.SingleToUInt32Bits(.5f), (uint)scene.Values[(20, "bloom_tint.g")].Bits, "Tint vira o trio linear");
        Assert.Equal(2u, (uint)scene.Values[(20, "dof_mode")].Bits, "Bokeh gravado pelo valor da Astra");
        Assert.Equal(BitConverter.SingleToUInt32Bits(.75f), (uint)scene.Values[(20, "weight")].Bits);
    }

    [Test]
    public static void RenderSettings_WritesTheGlobalVolumeAndMarksTheOverride()
    {
        var scene = Run("""
                var facade = Volume.Get(Object)!.Facade;
                facade.VolumeShape = EnvironmentVolume.VolumeShapeOption.Global;
                Volume.Get(Object)!.Enabled = true;
                RenderSettings.Fog = true;
                RenderSettings.FogDensity = .03f;
                RenderSettings.AmbientMode = AmbientMode.Flat;
                Say(RenderSettings.FogDensity);
                Say(RenderSettings.SceneVolume!.GameObject.ObjectId);
            """);
        Assert.Equal("0.03 2", string.Join(' ', scene.Logs));
        Assert.Equal(1u, (uint)scene.Values[(20, "override_fog")].Bits, "neblina no perfil");
        Assert.True(scene.Overridden.Contains(VolumeParameterTable.IndexOf("fog_density")), "caixa da densidade marcada");
        Assert.Equal(2u, (uint)scene.Values[(20, "ambient_source")].Bits, "Flat = Cor");
    }

    [Test]
    public static void UnityNames_MapToTheGeneratedFacadeAndMaterialSlot()
    {
        var scene = Run("""
                var world = SceneQuery.Root;
                var mesh = SceneQuery.FindFirstObjectByType<MeshRenderer>()!.Value;
                mesh.ShadowCastingMode = ShadowCastingMode.TwoSided;
                mesh.ReflectionProbeUsage = ReflectionProbeUsage.BlendProbesAndSkybox;
                mesh.Material.SetFloat("_Smoothness", .8f);
                mesh.Material.SetColor("_EmissionColor", new Color(2, 1, 0));
                try { mesh.Material.SetFloat("_Nada", 1); } catch (System.ArgumentException) { Say("recusado"); }
                var camera = SceneQuery.FindFirstObjectByType<Camera>()!.Value;
                camera.ClearFlags = CameraClearFlags.SolidColor;
                camera.FieldOfView = 50;
                camera.Rect = new Vector4(0, 0, .5f, 1);
                camera.RenderPostProcessing = true;
                var light = SceneQuery.FindFirstObjectByType<Light>()!.Value;
                light.Type = LightType.Spot;
                light.Shadows = LightShadows.Soft;
                var probe = SceneQuery.FindFirstObjectByType<ReflectionProbe>()!.Value;
                probe.Mode = ReflectionProbeMode.Realtime;
                probe.RefreshModeUnity = ReflectionProbeRefreshMode.ViaScripting;
                probe.RenderProbe();
                var time = TimeOfDay.Current!.Value;
                time.Speed = 60;
                time.Hour = 18.5f;
                Say(time.DayLengthMinutes);
                Say(SceneQuery.FindObjectsByType<Light>().Length);
            """);
        Assert.Equal("recusado 24 1", string.Join(' ', scene.Logs));
        Assert.Equal(2u, (uint)scene.Values[(30, "cast_shadows")].Bits);
        Assert.Equal(2u, (uint)scene.Values[(30, "reflection_probes")].Bits);
        Assert.Close(.2f, BitConverter.UInt32BitsToSingle((uint)scene.Slots[(30, "material.roughness", 0)].Bits), what: "_Smoothness = 1 - rugosidade");
        Assert.Close(.5f, BitConverter.UInt32BitsToSingle((uint)scene.Slots[(30, "material.emission.g", 0)].Bits), what: "emissão HDR normalizada");
        Assert.Close(2f, BitConverter.UInt32BitsToSingle((uint)scene.Slots[(30, "material.emission_strength", 0)].Bits), what: "o pico vira força");
        Assert.Equal(1u, (uint)scene.Values[(31, "background_type")].Bits, "SolidColor");
        Assert.Close(50f, BitConverter.UInt32BitsToSingle((uint)scene.Values[(31, "vertical_fov")].Bits));
        Assert.Close(.5f, BitConverter.UInt32BitsToSingle((uint)scene.Values[(31, "viewport_width")].Bits));
        Assert.Equal(2u, (uint)scene.Values[(41, "kind")].Bits, "Spot");
        Assert.Equal(2u, (uint)scene.Values[(41, "shadow_mode")].Bits, "Soft");
        Assert.Equal(1u, (uint)scene.Values[(42, "type")].Bits, "Realtime");
        Assert.Equal(2u, (uint)scene.Values[(42, "refresh_mode")].Bits, "Via scripting");
        Assert.True(scene.Calls.Contains("render_probe"), "RenderProbe chega ao método nativo");
        Assert.Close(18.5f, BitConverter.UInt32BitsToSingle((uint)scene.Values[(40, "hour")].Bits));
    }

    // Os 6 exemplos do G7 (examples/g7-api/Scripts) compilam pelo mesmo
    // compilador do aparelho e publicam os campos que a ferramenta de cena preenche.
    [Test]
    public static void Examples_CompileWithTheProjectCompilerAndPublishTheirFields()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null && !Directory.Exists(Path.Combine(directory.FullName, "examples", "g7-api", "Scripts")))
            directory = directory.Parent;
        Assert.True(directory is not null, "raiz do repositório localizada");
        var root = Path.Combine(Path.GetTempPath(), "astra-g7-examples-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(root, "Scripts"));
        try
        {
            foreach (var file in Directory.GetFiles(Path.Combine(directory!.FullName, "examples", "g7-api", "Scripts"), "*.cs"))
                File.Copy(file, Path.Combine(root, "Scripts", Path.GetFileName(file)));
            var result = new ProjectCompiler().Build(root);
            Assert.True(result.Success, string.Join("\n", result.Diagnostics.Select(d => d.File + ":" + d.Line + " " + d.Message)));
            var types = result.Project!.Types.ToDictionary(t => t.Id);
            Assert.Equal(6, types.Count, "seis exemplos");
            Assert.Equal("object", types["g7.perfil_por_area"].Properties.Single(p => p.Id == "volume").ValueType);
            Assert.Equal("asset", types["g7.perfil_por_area"].Properties.Single(p => p.Id == "perfil_da_area").ValueType);
            Assert.Equal("asset", types["g7.troca_de_material"].Properties.Single(p => p.Id == "material_b").ValueType);
            Assert.Equal("object", types["g7.mira_com_desfoque"].Properties.Single(p => p.Id == "volume_de_mira").ValueType);
            Assert.True(types.ContainsKey("g7.dia_acelerado") && types.ContainsKey("g7.postes_ao_anoitecer") &&
                        types.ContainsKey("g7.sonda_por_script"), "ids publicados");
        }
        finally { Directory.Delete(root, true); }
    }

    [Test]
    public static void AssetMethods_SendTheGuidInTwoIntegerHalves()
    {
        var scene = Run("""
                var volume = Volume.Get(Object)!;
                volume.Facade.SetSharedProfile(new AssetGuid(0x0123456789abcdef, 0xfedcba9876543210));
                volume.Facade.SetWeight(.5);
                SceneQuery.FindFirstObjectByType<MeshRenderer>()!.Value.SetSharedMaterial(new AssetGuid(1, 2));
            """);
        Assert.Equal(unchecked("set_shared_profile(" + (long)0x0123456789abcdef + "," + (long)0xfedcba9876543210 + ")"), scene.Calls[0]);
        Assert.Equal("set_weight(0.5)", scene.Calls[1]);
        Assert.Equal("set_shared_material(1,2)", scene.Calls[2]);
    }
}
