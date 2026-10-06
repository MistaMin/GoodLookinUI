// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
#include <goodlookinui/Design.h>
#include <cassert>
#include <cctype>
#include <set>
#include <iostream>
int main(){
 using namespace goodlookinui;
 Item i{"gain","preampGain","Gain, \"input\"","console","#AAbb01",12,15,70,76,11};
 std::stringstream file;writeDesign(file,{i});assert(readDesign(file)==std::vector<Item>{i});
 i.style="console";std::stringstream console;writeDesign(console,{i});assert(readDesign(console)==std::vector<Item>{i});
 auto legacyText=file.str();legacyText.replace(0,std::string("GoodLookinUI").size(),"HardwareUI");std::stringstream legacy(legacyText);assert(readDesign(legacy)==std::vector<Item>{i});
 auto bad=[&](std::string text){std::stringstream s(text);try{readDesign(s);}catch(...){return;}throw std::runtime_error("Accepted bad file");};
 bad(std::string("GoodLookinUI design version 1\n")+header+"\n\"unclosed,a,a,metal,#ffffff,0,0,70,76,10\n");
 bad("GoodLookinUI design version 2\n");
 std::stringstream duplicate;duplicate<<"GoodLookinUI design version 1\n"<<header<<"\na,a,a,metal,#ffffff,0,0,70,76,10\na,a,a,metal,#ffffff,0,0,70,76,10\n";bad(duplicate.str());
 for(auto value:{"nan","inf","12junk","-1"})bad(std::string("GoodLookinUI design version 1\n")+header+"\na,a,a,metal,#ffffff,0,0,"+value+",76,10\n");
 for(const auto& s:knobStyles){Item j=i;j.style=s.code;std::stringstream f;writeDesign(f,{j});assert(readDesign(f)==std::vector<Item>{j});}
 assert(findStyle("metal")->style==KnobStyle::Mtl && findStyle("bakelite")->style==KnobStyle::Bk && findStyle("ivory")->style==KnobStyle::Slv && findStyle("console")->style==KnobStyle::Brit);
 assert(!findStyle("bogus"));
 assert(findFaceplate("Navy") && findFaceplate("Graphite") && !findFaceplate("Nope") && std::size(faceplates)==40);
 {std::set<std::string> codes;
  auto banned=[](std::string s){for(auto& c:s)c=char(std::tolower(c));
    for(auto w:{"neve","ssl","api","focusrite","pultec","teletronix","dbx","spl","1176","la-2a","corvette","honda","commodore","arasaka","militech","netwatch","kiroshi","edgerunner","radial","crawford"})
      if(s.find(w)!=std::string::npos)return true;return false;};
  for(const auto& f:faceplates){assert(codes.insert(f.code).second);assert(f.finish>=0&&f.finish<=7);assert(!banned(f.code)&&!banned(f.name));}
  for(const auto& k:knobStyles){assert(!banned(k.code)&&!banned(k.description));}}
 // printed-mark option: round trip, legacy 10-column files still load as "auto", bad values rejected
 {Item mk=i;for(auto m:{"auto","light","dark"}){mk.marks=m;std::stringstream f;writeDesign(f,{mk});assert(readDesign(f)==std::vector<Item>{mk});}
  std::stringstream legacyFile;legacyFile<<"GoodLookinUI design version 1\n"<<headerV1<<"\na,a,Gain,Brit,#ffffff,1,2,70,76,10\n";
  auto old=readDesign(legacyFile);assert(old.size()==1&&old[0].marks=="auto"&&old[0].style=="Brit");
  Item badMarks=i;badMarks.marks="blue";bool threw=false;try{validate(badMarks);}catch(...){threw=true;}assert(threw);
  std::stringstream wrongCols;wrongCols<<"GoodLookinUI design version 1\n"<<header<<"\na,a,Gain,Brit,#ffffff,1,2,70,76,10\n";threw=false;try{readDesign(wrongCols);}catch(...){threw=true;}assert(threw);}
 // minimal quoting + the marks column is only written when used
 {Item q=i;q.label="plain";std::stringstream a;writeDesign(a,{q});assert(a.str().find('"')==std::string::npos&&a.str().find("marks")==std::string::npos&&a.str().find(headerV1)!=std::string::npos);
  assert(readDesign(a)==std::vector<Item>{q});
  q.marks="dark";std::stringstream b;writeDesign(b,{q});assert(b.str().find("marks")!=std::string::npos&&readDesign(b)==std::vector<Item>{q});
  q.marks="auto";q.label="a, \"b\"";std::stringstream c;writeDesign(c,{q});assert(c.str().find("\"a, \"\"b\"\"\"")!=std::string::npos&&readDesign(c)==std::vector<Item>{q});
  // a file in the original format loads and is written back byte for byte
  const std::string original=std::string("GoodLookinUI design version 1\n")+headerV1+"\nlcFreq,lcFreq,FREQ,console,#CBCBBC,12,52,150,98,10\nlcQ,lcQ,Q,console,#CBCBBC,12,152,150,98,10\n";
  std::stringstream o(original);auto items=readDesign(o);std::stringstream back;writeDesign(back,items);assert(back.str()==original);}
 Motion m;for(int n=0;n<600;++n){m.step(1,1.0/60);assert(m.position>=0 && m.position<=1.00001);}assert(std::abs(m.position-1)<1e-8);
 std::cout<<"Design roundtrip, invalid input, duplicate IDs and motion checks passed\n";
}
