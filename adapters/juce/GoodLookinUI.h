// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
#pragma once
#include <goodlookinui/Design.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <fstream>
#include <functional>

namespace goodlookinui::juce_adapter {
inline juce::Colour colour(const std::string& hex) {
    return juce::Colour::fromString("ff" + juce::String(hex.substr(1)));
}
// Console hardware is generated at the component's current scale; no bitmaps.
inline void drawScrew(juce::Graphics& g, float x, float y) {
    auto r=juce::Rectangle<float>(8,8).withCentre({x,y});
    g.setColour(juce::Colour(0xff0d1113));g.fillEllipse(r.expanded(1));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff7c8589),x-3,y-3,juce::Colour(0xff252b2e),x+3,y+3,false));g.fillEllipse(r);
    g.setColour(juce::Colour(0xff121719));g.drawLine(x-2,y+1,x+2,y-1,1.3f);
}
inline void drawPanel(juce::Graphics& g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0x88000000));g.fillRoundedRectangle(r.translated(0,2),3);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff394247),r.getX(),r.getY(),
        juce::Colour(0xff2a3034),r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,2);
    g.setColour(juce::Colour(0xff101618));g.drawRoundedRectangle(r,2,1);
    g.setColour(juce::Colour(0x22ffffff));g.drawHorizontalLine(int(r.getY()+1),r.getX()+2,r.getRight()-2);
}
inline void drawKey(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, bool down) {
    g.setColour(juce::Colour(0xff131719));g.fillRoundedRectangle(r.expanded(2),3);
    auto face=r.translated(0,down?1.5f:0);
    g.setColour(juce::Colour(0xff080c0d));g.fillRoundedRectangle(face.translated(0,2),2);
    g.setGradientFill(juce::ColourGradient(base.brighter(0.18f),face.getX(),face.getY(),
        base.darker(down?0.30f:0.16f),face.getRight(),face.getBottom(),false));g.fillRoundedRectangle(face,2);
    g.setColour(juce::Colour(0x66ffffff));g.drawHorizontalLine(int(face.getY()+1),face.getX()+2,face.getRight()-2);
    g.setColour(juce::Colour(0x44000000));g.drawRoundedRectangle(face,2,0.8f);
}
inline void drawConsoleKnob(juce::Graphics& g,juce::Rectangle<float> bounds,double proportion,const Item& item,bool active) {
    const float d=juce::jmin(bounds.getWidth(),bounds.getHeight());
    auto outer=juce::Rectangle<float>(d,d).withCentre(bounds.getCentre());
    auto c=outer.getCentre();const float r=d*0.5f;
    // Printed calibration marks remain fixed while the grip and pointer rotate.
    g.setColour(juce::Colour(active?0xffc5cabf:0xff707a7b));
    for(int n=0;n<13;++n){float a=(-0.8f+1.6f*float(n)/12)*juce::MathConstants<float>::pi;
        juce::Point<float> v(std::sin(a),-std::cos(a));
        g.drawLine({c+v*r*0.88f,c+v*r*(n%3==0?1.0f:0.95f)},n%3==0?1.2f:0.7f);}
    auto body=outer.reduced(d*0.13f);
    for(int n=5;n>0;--n){g.setColour(juce::Colour(0x09000000));g.fillEllipse(body.expanded(float(n)*0.7f).translated(1,3));}
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff565c60),c.x-r,c.y-r,
        juce::Colour(0xff080a0c),c.x+r,c.y+r,false));g.fillEllipse(body);
    const float a=float((proportion*1.6-0.8)*juce::MathConstants<double>::pi);
    const float gripR=body.getWidth()*0.5f;
    for(int n=0;n<28;++n){float t=a+float(n)*juce::MathConstants<float>::twoPi/28;
        juce::Point<float> v(std::sin(t),-std::cos(t));
        auto q=c+v*gripR*0.92f;
        const float light=juce::jlimit(0.0f,1.0f,0.5f-0.35f*(v.x+v.y));
        g.setColour(juce::Colour(0xff080a0b).interpolatedWith(juce::Colour(0xff61686b),light));
        g.drawLine({c+v*gripR*0.73f,q},2.2f);}
    auto cap=body.reduced(d*0.055f);
    auto base=active?colour(item.colour):juce::Colour(0xff697170);
    g.setGradientFill(juce::ColourGradient(base.brighter(0.28f),cap.getX(),cap.getY(),
        base.darker(0.42f),cap.getRight(),cap.getBottom(),false));g.fillEllipse(cap);
    g.setColour(base.darker(0.6f));g.drawEllipse(cap,0.8f);
    auto face=cap.reduced(d*0.032f);
    g.setGradientFill(juce::ColourGradient(base.brighter(0.08f),face.getX(),face.getY(),
        base.darker(0.10f),face.getRight(),face.getBottom(),false));g.fillEllipse(face);
    g.setColour(juce::Colour(0x27ffffff));g.drawEllipse(face,0.7f);
    // Recessed white pointer cut into the coloured moulded cap.
    juce::Point<float> v(std::sin(a),-std::cos(a));
    auto tip=c+v*face.getWidth()*0.43f;auto tail=c+v*face.getWidth()*0.12f;
    g.setColour(juce::Colour(0x77000000));g.drawLine({tail.translated(0.7f,0.7f),tip.translated(0.7f,0.7f)},3.4f);
    g.setColour(juce::Colour(0xfff5f2df));g.drawLine({tail,tip},2.5f);
}
inline void drawKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                     double proportion, const Item& item, bool active) {
    if(item.style=="console") {drawConsoleKnob(g,bounds,proportion,item,active);return;}
    const float d=juce::jmin(bounds.getWidth(),bounds.getHeight());
    auto disc=juce::Rectangle<float>(d,d).withCentre(bounds.getCentre());
    const auto centre=disc.getCentre(); const float r=d*0.5f;
    g.setColour(juce::Colour(0x66000000));g.fillEllipse(disc.translated(2,4));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff707174),disc.getX(),disc.getY(),
        juce::Colour(0xff17181a),disc.getRight(),disc.getBottom(),false));g.fillEllipse(disc);
    g.setColour(juce::Colour(0xff999a9b));g.drawEllipse(disc.reduced(1),0.7f);
    auto cap=disc.reduced(r*0.17f);
    auto base=active?colour(item.colour):juce::Colour(0xff555555);
    if(item.style=="bakelite")base=base.darker(0.65f);
    if(item.style=="ivory")base=juce::Colour(0xffddd5ba).interpolatedWith(base,0.15f);
    g.setGradientFill(juce::ColourGradient(base.brighter(0.35f),cap.getX(),cap.getY(),
        base.darker(0.55f),cap.getRight(),cap.getBottom(),false));g.fillEllipse(cap);
    g.setColour(juce::Colour(0x44000000));
    for(int n=0;n<40;++n){const float a=float(n)*juce::MathConstants<float>::twoPi/40;
        auto p=centre+juce::Point<float>(std::sin(a),-std::cos(a))*(r*0.8f);
        auto q=centre+juce::Point<float>(std::sin(a),-std::cos(a))*(r*0.69f);
        g.drawLine({p,q},0.8f);}
    g.setColour(juce::Colour(0x55ffffff));g.drawEllipse(cap.reduced(1),0.6f);
    const float a=float((proportion*1.6-0.8)*juce::MathConstants<double>::pi);
    auto tip=centre+juce::Point<float>(std::sin(a),-std::cos(a))*(r*0.64f);
    auto tail=centre+juce::Point<float>(std::sin(a),-std::cos(a))*(r*0.24f);
    g.setColour(item.style=="ivory"?juce::Colour(0xff282521):juce::Colour(0xfffff4df));
    g.drawLine({tail,tip},juce::jmax(1.5f,d*0.045f));
}

#if GOODLOOKINUI_ENABLE_EDITOR
// Development-only inspector. It is not included in a release translation unit.
class Studio : public juce::Component {
public:
    struct Entry { Item item; juce::Component* component; std::function<void(const Item&)> apply; };
    void add(Item item,juce::Component& component,std::function<void(const Item&)> applyItem) {
        entries.push_back({std::move(item),&component,std::move(applyItem)});
        selector.addItem(entries.back().item.id,int(entries.size()));
        if(entries.size()==1) selector.setSelectedId(1,juce::sendNotificationSync);
    }
    Studio() {
        for(auto* c:std::vector<juce::Component*>{&selector,&style,&label,&hex,&font,&x,&y,&w,&h,&apply,&save,&load,&undo,&status})addAndMakeVisible(c);
        style.addItem("metal",1);style.addItem("bakelite",2);style.addItem("ivory",3);style.addItem("console",4);
        for(auto* t:{&label,&hex,&font,&x,&y,&w,&h})t->setSelectAllWhenFocused(true);
        apply.setButtonText("Apply");save.setButtonText("Save CSV");load.setButtonText("Load CSV");undo.setButtonText("Undo");
        selector.onChange=[this]{show();}; apply.onClick=[this]{edit();};undo.onClick=[this]{if(!history.empty()){restore(history.back());history.pop_back();}};
        save.onClick=[this]{choose(true);};load.onClick=[this]{choose(false);};
        status.setText("Control | Style | Label | #Colour | Font | X Y W H",juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff18222c));g.setColour(juce::Colour(0xffce9435));g.drawRect(getLocalBounds(),2);}
    void resized() override {
        auto row=getLocalBounds().reduced(8);auto top=row.removeFromTop(26);
        selector.setBounds(top.removeFromLeft(135));style.setBounds(top.removeFromLeft(90));label.setBounds(top.removeFromLeft(110));
        hex.setBounds(top.removeFromLeft(90));font.setBounds(top.removeFromLeft(45));
        for(auto* t:{&x,&y,&w,&h})t->setBounds(top.removeFromLeft(55));
        auto buttons=row.removeFromTop(28);for(auto* b:{&apply,&save,&load,&undo})b->setBounds(buttons.removeFromLeft(95).reduced(2));
        status.setBounds(row);
    }
private:
    std::vector<Entry> entries;std::vector<std::vector<Item>> history;
    juce::ComboBox selector,style;juce::TextEditor label,hex,font,x,y,w,h;
    juce::TextButton apply,save,load,undo;juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<Item> snapshot() const {std::vector<Item> out;for(auto& e:entries)out.push_back(e.item);return out;}
    void show(){auto n=selector.getSelectedId()-1;if(n<0||n>=int(entries.size()))return;auto& i=entries[size_t(n)].item;
        label.setText(i.label);hex.setText(i.colour);style.setSelectedId(i.style=="metal"?1:i.style=="bakelite"?2:i.style=="ivory"?3:4,juce::dontSendNotification);
        font.setText(juce::String(i.fontSize));x.setText(juce::String(i.x));y.setText(juce::String(i.y));w.setText(juce::String(i.width));h.setText(juce::String(i.height));}
    void restore(const std::vector<Item>& items){
        // Verify the entire file before applying; parameter identities cannot be changed by artwork edits.
        if(items.size()!=entries.size())throw std::runtime_error("Design must contain all registered controls");
        for(auto& e:entries){auto i=std::find_if(items.begin(),items.end(),[&](auto& v){return v.id==e.item.id;});
            if(i==items.end()||i->parameter!=e.item.parameter)throw std::runtime_error("Design parameter bindings do not match this plugin");validate(*i);}
        for(auto& e:entries){e.item=*std::find_if(items.begin(),items.end(),[&](auto& v){return v.id==e.item.id;});e.apply(e.item);
            e.component->setBounds(juce::roundToInt(e.item.x),juce::roundToInt(e.item.y),juce::roundToInt(e.item.width),juce::roundToInt(e.item.height));}show();}
    void edit(){try{auto n=selector.getSelectedId()-1;if(n<0)return;auto items=snapshot();auto& i=items.at(size_t(n));
        auto num=[](const juce::TextEditor& t){auto s=t.getText().toStdString();size_t end;float v=std::stof(s,&end);if(end!=s.size())throw std::runtime_error("Invalid number");return v;};
        i.label=label.getText().toStdString();i.colour=hex.getText().toStdString();i.style=style.getText().toStdString();
        i.fontSize=num(font);i.x=num(x);i.y=num(y);i.width=num(w);i.height=num(h);validate(i);
        auto before=snapshot();restore(items);history.push_back(before);status.setText("Applied. DSP bindings preserved.",juce::dontSendNotification);
        }catch(const std::exception& e){status.setText(e.what(),juce::dontSendNotification);}}
    void choose(bool writing){chooser=std::make_unique<juce::FileChooser>(writing?"Save design":"Load design",juce::File{},"*.csv");
        auto flags=writing?(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting):juce::FileBrowserComponent::openMode;
        chooser->launchAsync(flags|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<Studio>(this),writing](const juce::FileChooser& c){
            if(!safe||c.getResult()==juce::File{})return;
            try{if(writing){std::ostringstream out;writeDesign(out,safe->snapshot());if(!c.getResult().replaceWithText(out.str()))throw std::runtime_error("Save failed");}
                else{std::ifstream in(c.getResult().getFullPathName().toStdString());auto items=readDesign(in);auto before=safe->snapshot();safe->restore(items);safe->history.push_back(before);}
                safe->status.setText(writing?"Design saved":"Design loaded",juce::dontSendNotification);
            }catch(const std::exception& e){safe->status.setText(e.what(),juce::dontSendNotification);}});
    }
};
#endif
}
