// Fachada C# tipada, gerada do registro de schemas.
//
// Sem isto, cada componente novo exigiria escrever à mão um `readonly struct`
// com os ids de propriedade copiados como string — o mesmo defeito das duas
// listas que o schema comum eliminou: o id muda no C++ e o C# continua
// compilando com o id velho, falhando só no aparelho. Aqui o texto do arquivo
// `managed/Astra.Scripting/Generated/Components.g.cs` sai dos descritores; um
// teste compara o arquivo do repositório com esta geração e falha quando o
// schema mudou sem regenerar:
//
//   aether_tests --write-component-api managed/Astra.Scripting/Generated/Components.g.cs
//
// A fachada usa o acesso genérico por id já exposto pela ABI (`Component.GetFloat`
// e afins). Nenhuma função nova atravessa a fronteira nativa por tipo.
#pragma once
#include "scene/component_schema.h"

#include <string>
#include <string_view>
#include <vector>

namespace ae::scene {
namespace csharp {
// snake_case → PascalCase; ids de propriedade são ASCII por contrato.
inline std::string pascal(std::string_view id) {
  std::string out;bool upper=true;
  for(const char c:id) {
    if(c=='_'||c=='-'||c=='.'||c==' ') {upper=true;continue;}
    out.push_back(upper&&c>='a'&&c<='z'?static_cast<char>(c-'a'+'A'):c);
    upper=false;
  }
  if(!out.empty() && out[0]>='0' && out[0]<='9') out.insert(out.begin(),'V');
  return out;
}
// Rótulos de opção são texto de interface em português ("Estático"): viram
// identificador sem acento, em PascalCase.
inline std::string identifier(std::string_view label) {
  std::string ascii;
  for(usize i=0;i<label.size();++i) {
    const unsigned char c=static_cast<unsigned char>(label[i]);
    if(c<0x80) {ascii.push_back(static_cast<char>(c));continue;}
    // Latin-1 em UTF-8 (0xC3 xx): a letra base sem o acento.
    if(c==0xC3 && i+1<label.size()) {
      const unsigned char n=static_cast<unsigned char>(label[++i]);
      // Índice = segundo byte - 0x80; espaço onde não há letra latina.
      static constexpr char base[]="AAAAAAACEEEEIIII NOOOOO OUUUUY  aaaaaaaceeeeiiii nooooo ouuuuy y";
      ascii.push_back(n>=0x80 && n<0x80+sizeof(base)-1?base[n-0x80]:' ');
      continue;
    }
    ascii.push_back(' ');
  }
  std::string out;bool upper=true;
  for(const char c:ascii) {
    const bool alnum=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
    if(!alnum) {upper=true;continue;}
    out.push_back(upper&&c>='a'&&c<='z'?static_cast<char>(c-'a'+'A'):c);
    upper=false;
  }
  if(out.empty()) out="Opcao";
  if(out[0]>='0'&&out[0]<='9') out.insert(out.begin(),'V');
  return out;
}
inline std::string xml(std::string_view text) {
  std::string out;
  for(const char c:text) {
    if(c=='<') out+="&lt;"; else if(c=='>') out+="&gt;"; else if(c=='&') out+="&amp;"; else out.push_back(c);
  }
  return out;
}
inline std::string number(float value) {
  std::string text=std::to_string(value);
  while(text.size()>1 && text.back()=='0') text.pop_back();
  if(!text.empty() && text.back()=='.') text.pop_back();
  return text;
}
} // namespace csharp

inline std::string componentCSharpApi() {
  using namespace csharp;
  std::string out=
    "// GERADO por scene::componentCSharpApi() a partir de native/scene/schemas/*.h — não edite.\n"
    "// Regenerar: aether_tests --write-component-api managed/Astra.Scripting/Generated/Components.g.cs\n"
    "#nullable enable\n"
    "using System.Numerics;\n\n"
    "namespace Astra.Components;\n\n"
    "/// <summary>Fachada tipada de um tipo de componente nativo.</summary>\n"
    "public interface IComponentFacade<TSelf> where TSelf : struct, IComponentFacade<TSelf>\n"
    "{\n"
    "    static abstract string TypeId { get; }\n"
    "    static abstract TSelf Wrap(Component component);\n"
    "    Component Component { get; }\n"
    "}\n\n"
    "/// <summary>Acesso tipado a partir do objeto: <c>obj.GetComponent&lt;PhysicsBody&gt;()</c>.</summary>\n"
    "public static class ComponentFacadeExtensions\n"
    "{\n"
    "    public static T? GetComponent<T>(this GameObject owner, int ordinal = 0) where T : struct, IComponentFacade<T> =>\n"
    "        owner.GetComponent(T.TypeId, ordinal) is { } component ? T.Wrap(component) : null;\n"
    "    public static T AddComponent<T>(this GameObject owner) where T : struct, IComponentFacade<T> =>\n"
    "        T.Wrap(owner.AddComponent(T.TypeId));\n"
    "    public static bool HasComponent<T>(this GameObject owner) where T : struct, IComponentFacade<T> =>\n"
    "        owner.HasComponent(T.TypeId);\n"
    "}\n";
  for(const auto &schema:componentSchemas) {
    if(schema.apiName.empty()) continue;
    const auto &type=*schema.type;
    const std::string name(schema.apiName);
    out+="\n/// <summary>"+xml(schema.name)+": "+xml(schema.description)+". Família "+
         xml(componentFamilyName(schema.family))+" · "+xml(schema.subfamily)+".</summary>\n";
    if(!schema.reference.empty()) out+="/// <remarks>Referência estudada: "+xml(schema.reference)+"</remarks>\n";
    out+="public readonly struct "+name+" : IComponentFacade<"+name+">\n{\n";
    out+="    public static string TypeId => \""+std::string(type.id)+"\";\n";
    out+="    public static "+name+" Wrap(Component component) => new(component);\n";
    out+="    public Component Component { get; }\n";
    out+="    public "+name+"(Component component)\n    {\n"
         "        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, \"tipo \" + component.TypeId);\n"
         "        Component = component;\n    }\n";
    out+="    public ulong InstanceId => Component.InstanceId;\n";
    // Canais de uma tripla saem como Vector3 único; os números avulsos, um a um.
    std::vector<std::string_view> channels;
    for(const auto &triple:type.triples) for(const auto channel:triple.channels) channels.push_back(channel);
    const auto doc=[&](const char *label,std::string_view unit,std::string_view help) {
      std::string text=std::string("    /// <summary>")+xml(label);
      if(!unit.empty()) text+=" ("+xml(unit)+")";
      if(!help.empty()) text+=". "+xml(help);
      return text+"</summary>\n";
    };
    for(const auto &triple:type.triples) {
      out+=doc(triple.name,{},triple.kind==ComponentTripleKind::LinearColor?"Cor linear RGB":"");
      out+="    public Vector3 "+pascal(triple.id)+"\n    {\n        get => new(";
      for(u32 axis=0;axis<3;++axis) out+=std::string(axis?", ":"")+"Component.GetFloat(\""+std::string(triple.channels[axis])+"\")";
      out+=");\n        set\n        {\n";
      const char *fields[]{"X","Y","Z"};
      for(u32 axis=0;axis<3;++axis)
        out+="            Component.SetFloat(\""+std::string(triple.channels[axis])+"\", value."+fields[axis]+");\n";
      out+="        }\n    }\n";
    }
    for(const auto &p:type.numbers) {
      if(p.id.empty() || std::find(channels.begin(),channels.end(),p.id)!=channels.end()) continue;
      out+=doc(p.name,p.presentation.unit,p.presentation.help?p.presentation.help:"");
      out+="    /// <remarks>Faixa válida: "+number(p.minimum)+" a "+number(p.maximum)+".</remarks>\n";
      out+="    public float "+pascal(p.id)+"\n    {\n        get => Component.GetFloat(\""+std::string(p.id)+
           "\");\n        set => Component.SetFloat(\""+std::string(p.id)+"\", value);\n    }\n";
    }
    for(const auto &p:type.booleans) {
      out+=doc(p.name,{},p.presentation.help?p.presentation.help:"");
      out+="    public bool "+pascal(p.id)+"\n    {\n        get => Component.GetBool(\""+std::string(p.id)+
           "\");\n        set => Component.SetBool(\""+std::string(p.id)+"\", value);\n    }\n";
    }
    for(const auto &p:type.enums) {
      const std::string enumName=pascal(p.id)+"Option";
      out+="    public enum "+enumName+" : uint\n    {\n";
      std::vector<std::string> used;
      for(const auto &option:p.options) {
        std::string label=identifier(option.name);
        if(std::find(used.begin(),used.end(),label)!=used.end()) label+=std::to_string(option.value);
        used.push_back(label);
        out+="        "+label+" = "+std::to_string(option.value)+",\n";
      }
      out+="    }\n";
      out+=doc(p.name,{},p.presentation.help?p.presentation.help:"");
      out+="    public "+enumName+" "+pascal(p.id)+"\n    {\n        get => ("+enumName+")Component.GetEnum(\""+
           std::string(p.id)+"\");\n        set => Component.SetEnum(\""+std::string(p.id)+"\", (uint)value);\n    }\n";
    }
    for(const auto &p:type.references) {
      out+=doc(p.name,{},p.presentation.help?p.presentation.help:"");
      out+="    public ObjectReference "+pascal(p.id)+"\n    {\n        get => Component.GetReference(\""+std::string(p.id)+
           "\");\n        set => Component.SetReference(\""+std::string(p.id)+"\", value);\n    }\n";
    }
    for(const auto &p:type.resourceBindings) {
      out+=doc(p.name,{},"Recurso do projeto por slot");
      out+="    public AssetGuid Get"+pascal(p.id)+"(uint slot = 0) => Component.GetResource(\""+std::string(p.id)+"\", slot);\n";
      out+="    public void Set"+pascal(p.id)+"(AssetGuid value, uint slot = 0) => Component.SetResource(\""+std::string(p.id)+"\", value, slot);\n";
    }
    for(const auto &p:type.slotNumbers) {
      out+=doc(p.name,p.presentation.unit,"Valor por slot");
      out+="    public float Get"+pascal(p.id)+"(uint slot) => Component.GetSlotFloat(\""+std::string(p.id)+"\", slot);\n";
      out+="    public void Set"+pascal(p.id)+"(uint slot, float value) => Component.SetSlotFloat(\""+std::string(p.id)+"\", value, slot);\n";
    }
    for(const auto &p:type.slotEnums) {
      out+=doc(p.name,{},"Opção por slot");
      out+="    public uint Get"+pascal(p.id)+"(uint slot) => Component.GetSlotEnum(\""+std::string(p.id)+"\", slot);\n";
      out+="    public void Set"+pascal(p.id)+"(uint slot, uint value) => Component.SetSlotEnum(\""+std::string(p.id)+"\", value, slot);\n";
    }
    out+="}\n";
  }
  return out;
}
} // namespace ae::scene
