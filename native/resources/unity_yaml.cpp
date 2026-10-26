#include "resources/unity_yaml.h"
#include <deque>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <set>
#include <tuple>

#include <cstdlib>
#include <cstring>
#include <string_view>
namespace {
// NDK libc++ nao implementa from_chars de ponto flutuante: strtod sobre
// copia terminada, recusando hex/inf/nan para manter o contrato decimal.
inline bool parseDecimal(std::string_view text,double &value) {
  if(text.empty()||text.size()>64||text.find_first_of("xXnNiI")!=text.npos)return false;
  char buffer[65];std::memcpy(buffer,text.data(),text.size());buffer[text.size()]=0;
  char *end=nullptr;value=std::strtod(buffer,&end);
  return end==buffer+text.size();
}
}
namespace ae::resources {
const UnityYamlNode *UnityYamlNode::get(std::string_view name) const {
  if(kind!=Kind::Map)return nullptr;
  for(const auto &entry:members)if(entry.first==name)return &entry.second;
  return nullptr;
}
bool UnityYamlNode::integer(i64 &out) const {
  if(kind!=Kind::Scalar||scalar.empty())return false;
  i64 value=0;const auto parsed=std::from_chars(scalar.data(),scalar.data()+scalar.size(),value);
  if(parsed.ec!=std::errc{}||parsed.ptr!=scalar.data()+scalar.size())return false;
  out=value;return true;
}
bool UnityYamlNode::number(double &out,bool allowInfinity) const {
  if(kind!=Kind::Scalar||scalar.empty())return false;
  double value=0;
  if(allowInfinity&&(scalar==".inf"||scalar=="Infinity"||scalar=="+.inf"))value=std::numeric_limits<double>::infinity();
  else if(allowInfinity&&(scalar=="-.inf"||scalar=="-Infinity"))value=-std::numeric_limits<double>::infinity();
  else if(!parseDecimal(scalar,value)||!std::isfinite(value))return false;
  out=value;return true;
}
bool UnityYamlNode::boolean(bool &out) const {i64 value=0;if(!integer(value)||(value!=0&&value!=1))return false;out=value!=0;return true;}
bool UnityYamlNode::empty() const {
  return kind==Kind::Scalar?scalar.empty():kind==Kind::Map?members.empty():elements.empty();
}
const UnityYamlObject *UnityYamlDocument::find(i64 fileId) const {
  for(const auto &object:objects)if(object.fileId==fileId)return &object;
  return nullptr;
}
namespace {
std::string_view trim(std::string_view text) {
  while(!text.empty()&&(text.front()==' '||text.front()=='\r'))text.remove_prefix(1);
  while(!text.empty()&&(text.back()==' '||text.back()=='\r'))text.remove_suffix(1);
  return text;
}
struct Line {std::string_view body;usize indent=0,number=0;};
class Reader {
public:
  std::vector<Line> lines;
  // Linhas de continuação unidas (a Unity quebra coleções longas, como
  // `{fileID: ..., guid: ...,` + `    type: 2}`); as views apontam para cá.
  std::deque<std::string> joined;
  usize at=0,nodes=0;
  std::string error;
  const std::function<bool()> &cancelled;
  explicit Reader(const std::function<bool()> &cancel):cancelled(cancel) {}
  bool fail(std::string_view reason) {
    if(error.empty())error="YAML Unity, linha "+std::to_string(at<lines.size()?lines[at].number:lines.empty()?1:lines.back().number)+": "+std::string(reason);
    return false;
  }
  bool tick(usize depth) {
    if(depth>UnityYamlDocument::MaximumDepth||++nodes>UnityYamlDocument::MaximumNodes)return fail("limite de profundidade ou elementos excedido.");
    return !(cancelled&&cancelled())||fail("leitura cancelada.");
  }
  bool collect(std::string_view text) {
    if(text.size()>UnityYamlDocument::MaximumBytes||text.find('\0')!=text.npos)return fail("fonte inválida ou acima de 64 MiB.");
    usize number=0;
    while(!text.empty()) {
      ++number;const auto end=text.find('\n');const auto raw=text.substr(0,end);text=end==text.npos?std::string_view{}:text.substr(end+1);
      usize indent=0;while(indent<raw.size()&&raw[indent]==' ')++indent;
      const auto body=trim(raw.substr(indent));
      if(body.empty()||body.front()=='#')continue;
      if(raw.find('\t')!=raw.npos)return fail("tabulação não permitida.");
      if(lines.size()>=UnityYamlDocument::MaximumNodes)return fail("limite de linhas excedido.");
      const usize first=number;
      if(int open=openCollections(body);open>0) {
        std::string whole(body);
        while(open>0&&!text.empty()) {
          ++number;const auto next=text.find('\n');const auto piece=trim(text.substr(0,next));
          text=next==text.npos?std::string_view{}:text.substr(next+1);
          if(piece.find('\t')!=piece.npos||whole.size()+piece.size()>65536)return fail("coleção em várias linhas inválida.");
          whole+=' ';whole+=piece;open+=openCollections(piece,false);
        }
        joined.push_back(std::move(whole));lines.push_back({joined.back(),indent,first});continue;
      }
      lines.push_back({body,indent,number});
    }
    return true;
  }
  // Coleções `{`/`[` abertas e não fechadas na linha. Na primeira linha só
  // conta quando o valor começa uma coleção (texto puro pode ter chaves).
  static int openCollections(std::string_view body,bool start=true) {
    if(start) {
      auto value=body;
      if(value.starts_with("- "))value=trim(value.substr(2));
      if(!value.empty()&&value.front()!='{'&&value.front()!='[') {
        const auto split=colon(value);if(split==value.npos)return 0;
        value=trim(value.substr(split+1));
      }
      if(value.empty()||(value.front()!='{'&&value.front()!='['))return 0;
      body=value;
    }
    int depth=0;char quote=0;
    for(usize i=0;i<body.size();++i) {
      const char c=body[i];
      if(quote){if(c==quote)quote=0;continue;}
      if(c=='\''||c=='"')quote=c;else if(c=='{'||c=='[')++depth;else if(c=='}'||c==']')--depth;
    }
    return depth;
  }
  static usize colon(std::string_view value) {
    char quote=0;int nesting=0;
    for(usize i=0;i<value.size();++i) {
      const char c=value[i];
      if(quote) {if(c=='\\'&&quote=='"'){++i;continue;}if(c==quote){if(quote=='\''&&i+1<value.size()&&value[i+1]=='\''){++i;continue;}quote=0;}continue;}
      if(c=='\''||c=='"'){quote=c;continue;}
      if(c=='{'||c=='[')++nesting;else if(c=='}'||c==']')--nesting;
      else if(c==':'&&!nesting&&(i+1==value.size()||value[i+1]==' '))return i;
    }
    return value.npos;
  }
  bool quoted(std::string_view &input,std::string &out) {
    const char delimiter=input.front();input.remove_prefix(1);out.clear();
    while(!input.empty()) {
      const char c=input.front();input.remove_prefix(1);
      if(c==delimiter) {if(delimiter=='\''&&!input.empty()&&input.front()=='\''){out+='\'';input.remove_prefix(1);continue;}return true;}
      if(c=='\\'&&delimiter=='"') {
        if(input.empty())return fail("escape incompleto.");
        const char escaped=input.front();input.remove_prefix(1);
        if(escaped=='n')out+='\n';else if(escaped=='r')out+='\r';else if(escaped=='t')out+='\t';
        else if(escaped=='"'||escaped=='\\'||escaped=='/')out+=escaped;
        else if(escaped=='u') {
          if(input.size()<4)return fail("escape Unicode incompleto.");
          unsigned code=0;const auto parsed=std::from_chars(input.data(),input.data()+4,code,16);
          if(parsed.ec!=std::errc{}||parsed.ptr!=input.data()+4||!code||(code>=0xd800&&code<=0xdfff))return fail("escape Unicode inválido.");
          input.remove_prefix(4);
          if(code<0x80)out+=static_cast<char>(code);
          else if(code<0x800){out+=static_cast<char>(0xc0|(code>>6));out+=static_cast<char>(0x80|(code&63));}
          else {out+=static_cast<char>(0xe0|(code>>12));out+=static_cast<char>(0x80|((code>>6)&63));out+=static_cast<char>(0x80|(code&63));}
        } else return fail("escape não suportado.");
      } else out+=c;
    }
    return fail("string sem fechamento.");
  }
  bool flow(std::string_view &input,UnityYamlNode &out,usize depth) {
    if(!tick(depth))return false;
    input=trim(input);if(input.empty())return true;
    const char first=input.front();
    if(first=='{'||first=='[') {
      out.kind=first=='{'?UnityYamlNode::Kind::Map:UnityYamlNode::Kind::Sequence;
      const char close=first=='{'?'}':']';input.remove_prefix(1);input=trim(input);
      if(!input.empty()&&input.front()==close){input.remove_prefix(1);return true;}
      for(;;) {
        std::string key;UnityYamlNode value;
        if(first=='{') {
          if(input.empty())return fail("mapa sem fechamento.");
          if(input.front()=='\''||input.front()=='"'){if(!quoted(input,key))return false;input=trim(input);if(input.empty()||input.front()!=':')return fail("chave sem dois-pontos.");input.remove_prefix(1);}
          else {const auto end=input.find(':');if(end==input.npos)return fail("chave sem dois-pontos.");key=std::string(trim(input.substr(0,end)));input.remove_prefix(end+1);}
          if(key.empty()||std::any_of(out.members.begin(),out.members.end(),[&](const auto &entry){return entry.first==key;}))return fail("chave vazia ou duplicada.");
        }
        if(!flow(input,value,depth+1))return false;
        if(first=='{')out.members.emplace_back(std::move(key),std::move(value));else out.elements.push_back(std::move(value));
        input=trim(input);if(input.empty())return fail("coleção sem fechamento.");
        const char next=input.front();input.remove_prefix(1);if(next==close)return true;
        if(next!=',')return fail("separador inválido.");
        input=trim(input);
      }
    }
    if(first=='\''||first=='"')return quoted(input,out.scalar);
    if(first=='&'||first=='*'||first=='!'||first=='|'||first=='>')return fail("aliases, tags e escalares de bloco não suportados.");
    const auto end=input.find_first_of(",]}");out.scalar=std::string(trim(input.substr(0,end)));input=end==input.npos?std::string_view{}:input.substr(end);
    return true;
  }
  bool value(std::string_view text,UnityYamlNode &out,usize depth) {
    text=trim(text);
    // A colon in a plain scalar is legal (paths, diagnostic text). Flow
    // punctuation is special only when the value begins a flow collection.
    if(!text.empty()&&text.front()!='{'&&text.front()!='['&&text.front()!='\''&&text.front()!='"') {
      if(text.front()=='&'||text.front()=='*'||text.front()=='!'||text.front()=='|'||text.front()=='>')return fail("escalar de bloco/tag não suportado.");
      if(!tick(depth))return false;
      out.scalar=std::string(text);return true;
    }
    if(!flow(text,out,depth))return false;
    text=trim(text);return text.empty()||text.front()=='#'||fail("texto após valor.");
  }
  bool member(UnityYamlNode &map,std::string_view body,usize indent,usize depth) {
    const auto split=colon(body);if(split==body.npos)return fail("entrada de mapa sem dois-pontos.");
    auto name=trim(body.substr(0,split));std::string key;
    if(!name.empty()&&(name.front()=='\''||name.front()=='"')){if(!quoted(name,key)||!trim(name).empty())return fail("chave inválida.");}else key=std::string(name);
    if(key.empty()||key=="<<"||std::any_of(map.members.begin(),map.members.end(),[&](const auto &entry){return entry.first==key;}))return fail("chave vazia, duplicada ou merge não suportado.");
    UnityYamlNode child;const auto remainder=trim(body.substr(split+1));
    if(!remainder.empty()){if(!value(remainder,child,depth+1))return false;}
    else if(at<lines.size()&&(lines[at].indent>indent||(lines[at].indent==indent&&sequence(lines[at].body)))) {
      if(!block(child,lines[at].indent,depth+1))return false;
    }
    map.members.emplace_back(std::move(key),std::move(child));return true;
  }
  static bool sequence(std::string_view body) {return body=="-"||body.starts_with("- ");}
  bool block(UnityYamlNode &out,usize indent,usize depth) {
    if(!tick(depth)||at>=lines.size())return false;
    const bool list=sequence(lines[at].body);out.kind=list?UnityYamlNode::Kind::Sequence:UnityYamlNode::Kind::Map;
    while(at<lines.size()&&lines[at].indent==indent&&!lines[at].body.starts_with("---")) {
      if(sequence(lines[at].body)!=list)break;
      const auto body=lines[at++].body;
      if(!list){if(!member(out,body,indent,depth))return false;continue;}
      UnityYamlNode child;const auto remainder=trim(body.substr(1));
      if(remainder.empty()) {
        if(at<lines.size()&&lines[at].indent>indent) {if(!block(child,lines[at].indent,depth+1))return false;}
      } else if(colon(remainder)!=remainder.npos&&remainder.front()!='{') {
        child.kind=UnityYamlNode::Kind::Map;
        if(!member(child,remainder,indent+2,depth+1))return false;
        while(at<lines.size()&&lines[at].indent>indent) {
          if(lines[at].indent!=indent+2||sequence(lines[at].body))return fail("indentação de item inválida.");
          const auto next=lines[at++].body;if(!member(child,next,indent+2,depth+1))return false;
        }
      } else if(!value(remainder,child,depth+1))return false;
      out.elements.push_back(std::move(child));
    }
    return true;
  }
};
}
bool readUnityMeta(std::string_view text,UnityYamlNode &out,std::string &diagnostic) {
  const std::function<bool()> cancel;Reader reader(cancel);UnityYamlNode candidate;
  if(!reader.collect(text)||reader.lines.empty()||reader.lines.front().indent||!reader.block(candidate,0,0)||reader.at!=reader.lines.size()) {
    diagnostic=reader.error.empty()?"Metadados Unity inválidos.":reader.error;return false;}
  out=std::move(candidate);diagnostic.clear();return true;
}
bool readUnityYaml(std::string_view text,UnityYamlDocument &out,std::string &diagnostic,const std::function<bool()> &cancelled) {
  Reader reader(cancelled);UnityYamlDocument candidate;std::set<i64> identities;
  const auto fail=[&](std::string_view reason){reader.fail(reason);diagnostic=reader.error;return false;};
  if(!reader.collect(text)){diagnostic=reader.error;return false;}
  while(reader.at<reader.lines.size()&&reader.lines[reader.at].body.starts_with('%'))++reader.at;
  while(reader.at<reader.lines.size()) {
    if(candidate.objects.size()>=UnityYamlDocument::MaximumObjects)return fail("mais de 65536 objetos.");
    const auto header=reader.lines[reader.at++];if(header.indent||!header.body.starts_with("--- !u!"))return fail("cabeçalho de objeto Unity ausente.");
    auto fields=header.body.substr(7);const auto space=fields.find(' ');if(space==fields.npos)return fail("classe sem fileID.");
    UnityYamlObject object;UnityYamlNode number;number.scalar=std::string(fields.substr(0,space));i64 classId=0;
    if(!number.integer(classId)||classId<=0||classId>std::numeric_limits<u32>::max())return fail("classID inválido.");
    object.classId=static_cast<u32>(classId);fields=trim(fields.substr(space+1));if(fields.empty()||fields.front()!='&')return fail("fileID ausente.");
    fields.remove_prefix(1);const auto end=fields.find(' ');number.scalar=std::string(fields.substr(0,end));
    if(!number.integer(object.fileId)||!object.fileId||!identities.insert(object.fileId).second)return fail("fileID inválido ou duplicado.");
    if(end!=fields.npos){if(trim(fields.substr(end+1))!="stripped")return fail("sufixo de objeto desconhecido.");object.stripped=true;}
    UnityYamlNode root;
    if(reader.at>=reader.lines.size()||reader.lines[reader.at].indent||!reader.block(root,0,0)||root.kind!=UnityYamlNode::Kind::Map||root.members.size()!=1)return fail("objeto sem mapa raiz único.");
    object.type=std::move(root.members.front().first);object.value=std::move(root.members.front().second);
    if(object.value.kind!=UnityYamlNode::Kind::Map)return fail("objeto sem propriedades.");
    candidate.objects.push_back(std::move(object));
  }
  if(candidate.objects.empty())return fail("documento sem objetos.");
  out=std::move(candidate);diagnostic.clear();return true;
}
bool unityObjectReference(const UnityYamlNode &node,UnityObjectReference &out) {
  const auto *id=node.get("fileID");UnityObjectReference value;
  if(!id||!id->integer(value.fileId))return false;
  if(const auto *guid=node.get("guid")) {
    if(guid->kind!=UnityYamlNode::Kind::Scalar||guid->scalar.size()!=32)return false;
    if(guid->scalar!="00000000000000000000000000000000"&&!AssetGuid::parse(guid->scalar,value.guid))return false;
  }
  if(const auto *type=node.get("type")){i64 number=0;if(!type->integer(number)||number<0||number>3)return false;value.type=static_cast<u32>(number);}
  out=value;return true;
}
bool unityObjectReferences(const UnityYamlDocument &document,std::vector<UnityObjectReference> &out,std::string &diagnostic) {
  std::vector<UnityObjectReference> result;
  std::set<std::tuple<u64,u64,i64,u32>> unique;
  const auto visit=[&](const auto &self,const UnityYamlNode &node)->bool {
    if(node.get("fileID")) {
      UnityObjectReference reference;if(!unityObjectReference(node,reference)){diagnostic="Referência Unity inválida.";return false;}
      if(reference.fileId&&unique.emplace(reference.guid.high,reference.guid.low,reference.fileId,reference.type).second) {
        if(result.size()>=UnityYamlDocument::MaximumObjects){diagnostic="Mais de 65536 referências Unity.";return false;}
        result.push_back(reference);
      }
    }
    for(const auto &member:node.members)if(!self(self,member.second))return false;
    for(const auto &element:node.elements)if(!self(self,element))return false;
    return true;
  };
  for(const auto &object:document.objects)if(!visit(visit,object.value))return false;
  out=std::move(result);diagnostic.clear();return true;
}
} // namespace ae::resources
