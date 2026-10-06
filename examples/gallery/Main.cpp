// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
// Gallery app: shows every part the toolkit can draw. `--selftest <folder>` renders each page to a PNG,
// checks that it is not blank and that animation changes it, then exits (0 = pass).
#include <juce_gui_basics/juce_gui_basics.h>
#include <GoodLookinUI.h>
#include <Gallery.h>

class GalleryApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "GoodLookinUI Gallery"; }
    const juce::String getApplicationVersion() override { return "0.2.2"; }
    void initialise(const juce::String& commandLine) override {
        auto args = juce::StringArray::fromTokens(commandLine, true);
        if (args.size() >= 2 && args[0] == "--selftest") { selfTest(juce::File(args[1])); return; }
        if (args.size() >= 2 && args[0] == "--screenshots") { screenshots(juce::File(args[1])); return; }
        window = std::make_unique<Window>();
    }
    void shutdown() override { window.reset(); }
private:
    struct Window : juce::DocumentWindow {
        Window() : DocumentWindow("GoodLookinUI Gallery", juce::Colour(0xff14181c), allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new goodlookinui::juce_adapter::Gallery(), true); setResizable(true, true); centreWithSize(1280, 860); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    static long difference(const juce::Image& a, const juce::Image& b) {
        long n = 0; for (int y = 0; y < a.getHeight(); y += 2) for (int x = 0; x < a.getWidth(); x += 2) { auto p = a.getPixelAt(x, y), q = b.getPixelAt(x, y); n += std::abs(p.getRed() - q.getRed()) + std::abs(p.getGreen() - q.getGreen()) + std::abs(p.getBlue() - q.getBlue()); }
        return n;
    }
    // Renders every page at 2x for documentation (docs/images). The spectrogram gets extra time to fill.
    void screenshots(const juce::File& out) {
        out.createDirectory(); goodlookinui::juce_adapter::Gallery g;
        static const char* names[] = {"knobs", "plates", "meters", "sliders", "switches", "spectrum"};
        for (int p = 0; p < goodlookinui::juce_adapter::Gallery::numPages; ++p) {
            auto img = g.renderPage(p, p == 5 ? 16 : 5, 2.0f);
            juce::File f = out.getChildFile(juce::String(names[p]) + ".png"); f.deleteFile(); juce::FileOutputStream o(f); juce::PNGImageFormat().writeImageToStream(img, o);
            std::printf("wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        }
        setApplicationReturnValue(0); quit();
    }
    void selfTest(const juce::File& out) {
        out.createDirectory(); int failures = 0; goodlookinui::juce_adapter::Gallery g;
        for (int p = 0; p < goodlookinui::juce_adapter::Gallery::numPages; ++p) {
            auto first = g.renderPage(p, 1); auto later = g.renderPage(p, 2);
            const long blank = difference(first, juce::Image(juce::Image::ARGB, first.getWidth(), first.getHeight(), true));
            const long moved = difference(first, later);
            const bool ok = blank > 400000 && moved > 8000;
            std::printf("%s page %d (%s): content %ld, animation %ld\n", ok ? "PASS" : "FAIL", p, goodlookinui::juce_adapter::Gallery::pageName(p), blank, moved);
            if (!ok) ++failures;
            juce::File f = out.getChildFile("gallery_" + juce::String(p) + ".png"); f.deleteFile(); juce::FileOutputStream o(f); juce::PNGImageFormat().writeImageToStream(later, o);
        }
        // glow off must change the retro pages
        g.setPage(2); goodlookinui::juce_adapter::retro::glowEnabled = true; auto on = g.renderPage(2, 1); goodlookinui::juce_adapter::retro::glowEnabled = false; auto off = g.renderPage(2, 1);
        goodlookinui::juce_adapter::retro::glowEnabled = true;
        const bool glowOk = difference(on, off) > 20000; std::printf("%s glow switch changes the meters\n", glowOk ? "PASS" : "FAIL"); if (!glowOk) ++failures;
        failures += marksTest(out);
        setApplicationReturnValue(failures == 0 ? 0 : 1); quit();
    }
    // Printed marks must contrast with the plate. For every knob style, render the same knob with marks forced
    // light and forced dark: the pixels that differ ARE the marks, so the dark ones must be darker than the light
    // ones, and Auto must equal "dark" on a light plate and "light" on a dark plate, pixel for pixel.
    int marksTest(const juce::File& out) {
        using namespace goodlookinui;
        int bad = 0, checked = 0, withMarks = 0;
        auto render = [](int style, const char* marks, juce::Colour plate) {
            juce::Image img(juce::Image::ARGB, 200, 200, true); juce::Graphics g(img); g.fillAll(plate);
            Item it; it.style = knobStyles[style].code; it.colour = "#5793b8"; it.marks = marks;
            juce_adapter::drawKnob(g, juce::Rectangle<float>(0, 0, 200, 200), 0.5, it, true, plate); return img;
        };
        auto same = [](const juce::Image& a, const juce::Image& b) { for (int y = 0; y < a.getHeight(); ++y) for (int x = 0; x < a.getWidth(); ++x) if (a.getPixelAt(x, y) != b.getPixelAt(x, y)) return false; return true; };
        for (int s = 0; s < int(std::size(knobStyles)); ++s) for (int light = 0; light < 2; ++light) {
            const juce::Colour plate = light ? juce::Colour(0xffd9dcdc) : juce::Colour(0xff2b3236);
            auto lightMarks = render(s, "light", plate), darkMarks = render(s, "dark", plate), autoMarks = render(s, "auto", plate);
            double lightSum = 0, darkSum = 0; int n = 0;
            for (int y = 0; y < 200; ++y) for (int x = 0; x < 200; ++x) { const auto p = lightMarks.getPixelAt(x, y), q = darkMarks.getPixelAt(x, y);
                if (p != q) { lightSum += p.getPerceivedBrightness(); darkSum += q.getPerceivedBrightness(); ++n; } }
            if (n == 0) continue;                                               // this style prints no marks
            ++checked; if (light) ++withMarks;
            bool ok = darkSum < lightSum && same(autoMarks, light ? darkMarks : lightMarks);
            if (!ok) { ++bad; std::printf("FAIL marks: style %s on a %s plate (dark %.2f vs light %.2f over %d px, auto matches: %d)\n", knobStyles[s].code, light ? "light" : "dark", darkSum / n, lightSum / n, n, int(same(autoMarks, light ? darkMarks : lightMarks))); }
        }
        // picture: Brit on light and dark plates, three settings each
        juce::Image sheet(juce::Image::RGB, 960, 320, true); juce::Graphics sg(sheet);
        for (int light = 0; light < 2; ++light) for (int mode = 0; mode < 3; ++mode) {
            const juce::Colour plate = light ? juce::Colour(0xffc4c8c8) : juce::Colour(0xff2b3236); juce::Rectangle<float> cell(float(mode) * 320, float(light) * 160, 320, 160);
            sg.setColour(plate); sg.fillRect(cell); Item it; it.style = "Brit"; it.colour = "#5793b8"; it.marks = mode == 0 ? "auto" : mode == 1 ? "light" : "dark";
            juce_adapter::drawKnob(sg, cell.reduced(20, 8), 0.4, it, true, plate);
            sg.setColour(light ? juce::Colour(0xff222222) : juce::Colour(0xffdddddd)); sg.setFont(11.0f); sg.drawText(it.marks, cell.removeFromBottom(14), juce::Justification::centred);
        }
        juce::File f = out.getChildFile("marks.png"); f.deleteFile(); juce::FileOutputStream o(f); juce::PNGImageFormat().writeImageToStream(sheet, o);
        std::printf("%s printed-mark contrast (%d knob/plate combinations checked, %d styles print marks)\n", bad == 0 ? "PASS" : "FAIL", checked, withMarks);
        return bad;
    }
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(GalleryApp)
