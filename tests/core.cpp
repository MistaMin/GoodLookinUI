// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
#include <goodlookinui/Design.h>
#include <cassert>
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
 Motion m;for(int n=0;n<600;++n){m.step(1,1.0/60);assert(m.position>=0 && m.position<=1.00001);}assert(std::abs(m.position-1)<1e-8);
 std::cout<<"Design roundtrip, invalid input, duplicate IDs and motion checks passed\n";
}
