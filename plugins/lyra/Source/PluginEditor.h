#pragma once

#include "PluginProcessor.h"
#include "ChordEngine.h"

namespace lyra
{

struct Theme
{
    juce::Colour bg, panel, edge, text, dim, faint, hi, onHi, white, black;
    static Theme make (bool light, juce::Colour highlight);
    static const std::array<juce::Colour, 8>& swatches();
};

class Editor : public juce::AudioProcessorEditor,
               private juce::Timer
{
public:
    explicit Editor (Processor&);
    ~Editor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int baseWidth = 1180, baseHeight = 700;

private:
    struct Content : public juce::Component
    {
        explicit Content (Editor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        void mouseDown (const juce::MouseEvent& e) override { owner.contentMouseDown (e); }
        void mouseDrag (const juce::MouseEvent& e) override { owner.contentMouseDrag (e); }
        void mouseUp (const juce::MouseEvent&) override { owner.dragNote = -1; }
        Editor& owner;
    };
    struct Pill { juce::String label, value; juce::Rectangle<float> bounds; std::function<void()> click; bool accent = false; };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void paintBar (juce::Graphics&);
    void paintChord (juce::Graphics&, juce::Rectangle<float>);
    void paintStaff (juce::Graphics&, juce::Rectangle<float>);
    void paintWheel (juce::Graphics&, juce::Rectangle<float>);
    void paintPiano (juce::Graphics&, juce::Rectangle<float>);
    void contentMouseDown (const juce::MouseEvent&);
    void contentMouseDrag (const juce::MouseEvent&);
    int noteAt (juce::Point<float>) const;
    void buildPills();
    void showKeyMenu();
    void showSpellingMenu();
    void showColourMenu();
    void showLanguageMenu();
    void showActivation (bool);

    juce::Font ui (float h, bool bold = false) const;
    Language lang() const { return (Language) proc.language; }
    Key currentKey() const;
    SpellMode mode() const { return (SpellMode) proc.spelling; }

    Processor& proc;
    Content content { *this };
    Theme theme;
    std::vector<Pill> pills;

    // what is shown
    std::array<bool, 128> sounding {}, mouseNotes {};
    std::vector<int> notes, shownNotes;
    std::vector<ChordReading> readings;
    bool showingLast = false;
    int dragNote = -1;

    // automatic key
    std::array<float, 12> histogram {};
    Key autoKey { 0, false }, candidate { 0, false };
    int candidateFrames = 0;

    juce::Rectangle<float> pianoArea;
    lt::ui::Palette ltPalette = lt::ui::Palette::dark();
    std::unique_ptr<lt::ui::ActivationComponent> activation;
    static bool activationShownThisSession;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace lyra
