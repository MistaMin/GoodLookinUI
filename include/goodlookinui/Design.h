// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <limits>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace goodlookinui {
struct Item {
    std::string id, parameter, label, style = "metal";
    std::string colour = "#CE9435";
    float x=0, y=0, width=70, height=76, fontSize=10;
    bool operator==(const Item&) const = default;
};
inline void validate(const Item& i) {
    if (i.id.empty() || i.id.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error("Control needs a valid ID");
    if (i.style != "metal" && i.style != "bakelite" && i.style != "ivory" && i.style != "console")
        throw std::runtime_error("Unknown knob style: " + i.style);
    if (i.colour.size()!=7 || i.colour[0]!='#' ||
        i.colour.find_first_not_of("0123456789abcdefABCDEF",1)!=std::string::npos)
        throw std::runtime_error("Colour must be #RRGGBB");
    for (float v : {i.x,i.y,i.width,i.height,i.fontSize})
        if (!std::isfinite(v)) throw std::runtime_error("Geometry must be finite");
    if (i.width<20 || i.height<20 || i.width>4096 || i.height>4096 ||
        i.fontSize<6 || i.fontSize>72 || std::abs(i.x)>8192 || std::abs(i.y)>8192)
        throw std::runtime_error("Geometry is outside supported limits");
}
inline std::string quote(const std::string& s) {
    std::string out="\"";
    for (char c:s) { if(c=='\r'||c=='\n') throw std::runtime_error("Use single-line text");
        if(c=='\"') out+='\"'; out+=c; }
    return out+'\"';
}
inline std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out; std::string value; bool quoted=false, closed=false;
    for (std::size_t n=0;n<line.size();++n) {
        char c=line[n];
        if(quoted) { if(c=='\"') { if(n+1<line.size() && line[n+1]=='\"') {value+='\"';++n;}
            else {quoted=false;closed=true;} } else value+=c; }
        else if(c==',') {out.push_back(value);value.clear();closed=false;}
        else if(c=='\"' && value.empty() && !closed) quoted=true;
        else {if(closed || c=='\"') throw std::runtime_error("Malformed CSV quoting");value+=c;}
    }
    if(quoted) throw std::runtime_error("Unclosed CSV quote");
    out.push_back(value); return out;
}
inline constexpr auto header="id,parameter,label,style,colour,x,y,width,height,font size";
inline void writeDesign(std::ostream& out, const std::vector<Item>& items) {
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "GoodLookinUI design version 1\n" << header << '\n';
    std::set<std::string> ids;
    for(const auto& i:items) {validate(i); if(!ids.insert(i.id).second) throw std::runtime_error("Duplicate ID");
        out<<quote(i.id)<<','<<quote(i.parameter)<<','<<quote(i.label)<<','<<quote(i.style)<<','<<quote(i.colour)
           <<','<<i.x<<','<<i.y<<','<<i.width<<','<<i.height<<','<<i.fontSize<<'\n';}
    if(!out) throw std::runtime_error("Could not write design");
}
inline std::vector<Item> readDesign(std::istream& in) {
    std::string line; auto next=[&]{ std::getline(in,line); if(!line.empty()&&line.back()=='\r') line.pop_back(); };
    next(); if(line!="GoodLookinUI design version 1" && line!="HardwareUI design version 1") throw std::runtime_error("Unsupported design version");
    next(); if(line!=header) throw std::runtime_error("Invalid design columns");
    std::vector<Item> result; std::set<std::string> ids;
    while(std::getline(in,line)) {if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
        auto f=fields(line);if(f.size()!=10)throw std::runtime_error("Expected ten columns");
        auto number=[&](int n){std::size_t end=0;float v=std::stof(f[n],&end);
            if(end!=f[n].size())throw std::runtime_error("Invalid number");return v;};
        Item i{f[0],f[1],f[2],f[3],f[4],number(5),number(6),number(7),number(8),number(9)};
        validate(i);if(!ids.insert(i.id).second)throw std::runtime_error("Duplicate ID");result.push_back(i);
    }
    if(in.bad())throw std::runtime_error("Could not read design");return result;
}
// Critically damped motion; display animation never delays DSP parameter updates.
struct Motion {
    double position=0, velocity=0;
    void step(double target,double seconds,double frequency=18) {
        seconds=std::clamp(seconds,0.0,0.1);
        const double offset=position-target, decay=std::exp(-frequency*seconds);
        const double term=velocity+frequency*offset;
        position=target+(offset+term*seconds)*decay;
        velocity=(velocity-frequency*term*seconds)*decay;
    }
};
}
