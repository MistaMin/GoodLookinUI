#include <goodlookinui/ReadoutWindow.h>
#include <goodlookinui/Layout.h>
#include <cstdlib>
#include <limits>
// Always-on checks: Release builds define NDEBUG.
static void check(bool ok) { if (!ok) std::abort(); }
int main() {
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    for (double rate : {44100., 48000., 96000.}) {
        goodlookinui::ReadoutWindow a(rate), b(rate);
        check(a.loudness() == -200);
        a.add(-20, 12, 1000); a.add(-10, 6, 1000);
        check(std::abs(a.loudness() - -12.596373f) < 0.0001f); check(a.crest() == 12);
        a.add(-70, nan, 100); check(std::abs(a.loudness() - -12.596373f) < 0.0001f);
        a.add(std::numeric_limits<float>::infinity(), nan, std::uint64_t(rate * 2.1));
        check(a.loudness() == -200 && a.crest() == -100);
        a.add(-18, 7, 100); a.reset(); check(a.loudness() == -200);
        a.add(-23, 9, 12345); for (int i=0;i<12345;++i) b.add(-23, 9, 1);
        check(std::abs(a.loudness()-b.loudness()) < 0.0001f && a.crest() == b.crest());
    }
    check(goodlookinui::ReadoutWindow(nan).loudness() == -200);
    using namespace goodlookinui::layout;
    for (float h : {0.f, 12.f, 80.f, 400.f}) {
        Rect panel{10,20,100,h}; float bottom=panel.y;
        for (int i=0;i<5;++i) {
            auto card=readoutCard(panel,i,5,8); check(bool(card));
            check(card->y >= bottom-0.001f && card->y+card->h <= panel.y+panel.h+0.001f);
            check(card->x>=panel.x && card->x+card->w<=panel.x+panel.w); bottom=card->y+card->h;
        }
    }
    check(!readoutCard({0,0,100,100},0,0,8)); check(!readoutCard({0,0,-1,100},0,5,8));
    for (float width : {0.f,10.f,100.f}) {
        auto pair=captionSwitch({0,0,width,20},24,8); check(bool(pair));
        check(pair->first.x+pair->first.w<=pair->second.x && pair->second.x+pair->second.w<=width);
    }
}
