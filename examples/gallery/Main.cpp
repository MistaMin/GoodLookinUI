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
    const juce::String getApplicationVersion() override { return "0.2.1"; }
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
        setApplicationReturnValue(failures == 0 ? 0 : 1); quit();
    }
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(GalleryApp)
