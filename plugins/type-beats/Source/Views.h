#pragma once

#include <lt_shared/lt_shared.h>
#include "Analysis.h"

namespace typebeats
{

using lt::ui::Palette;

/** Base for views that draw with the current palette. */
struct ThemedView : public juce::Component
{
    const Palette* palette = nullptr;
    const Palette& pal() const { return *palette; }
};

// ------------------------------------------------------------------ waveform

class WaveformView : public ThemedView
{
public:
    void setResult (const AnalysisResult*, const juce::String& fileName);
    void setPlayhead (double seconds, bool playing);
    void setBusy (bool busy, float progress, const juce::String& status);

    std::function<void (double seconds)> onClick;     // toggles preview from that position
    std::function<void()> onBrowse;
    std::function<void()> onDragOut;                   // start dragging the audio file out

    void paint (juce::Graphics&) override;
    void resized() override { rebuildDust(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void rebuildDust();
    juce::Rectangle<float> fileChip() const;

    struct Dust { float x, y, size, alpha; };
    std::vector<Dust> dust;
    const AnalysisResult* result = nullptr;
    juce::String fileName, status;
    double playhead = 0;
    bool playing = false, busy = false, draggingOut = false;
    float progress = 0;
};

// ------------------------------------------------------------------ BPM

class BpmView : public ThemedView, private juce::Timer
{
public:
    BpmView();
    void setBpm (double bpm, bool tapped);
    double getBpm() const noexcept { return bpm; }
    void setActive (bool a) { active = a; }

    std::function<void (double)> onBpmChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::TextButton tapButton { "TAP" }, halfButton { "1/2" }, doubleButton { "x2" };

private:
    void timerCallback() override { repaint(); }
    void tap();

    double bpm = 0;
    bool tapped = false, active = false;
    std::vector<double> taps;
};

// ------------------------------------------------------------------ key

class KeyView : public ThemedView, private juce::Timer
{
public:
    KeyView();
    void setKeys (const std::vector<KeyCandidate>*);
    std::function<void (int index)> onChooseAlternative;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Point<float> moonPosition (int i) const;
    juce::Rectangle<float> moonLabel (int i) const;
    juce::Point<float> planetCentre() const;

    const std::vector<KeyCandidate>* keys = nullptr;
    int hoverMoon = -1;
};

// ------------------------------------------------------------------ piano

class PianoView : public ThemedView
{
public:
    void setActiveNotes (std::vector<int> pitchClasses, int bassPc);
    void paint (juce::Graphics&) override;

private:
    std::vector<int> active;
    int bass = -1;
};

// ------------------------------------------------------------------ chords

class ChordView : public ThemedView
{
public:
    ChordView();
    void setResult (AnalysisResult*);
    void setPlayhead (double seconds);
    int getSelected() const noexcept { return selected; }

    std::function<void()> onChordEdited;
    std::function<void (int index)> onSelect;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct StarPos { juce::Point<float> p; int index; };
    std::vector<StarPos> layoutStars() const;
    void showAlternatives (int index, juce::Point<float> at);
    void scroll (int delta);

    AnalysisResult* result = nullptr;
    int first = 0, selected = -1, current = -1;
    static constexpr int visible = 4;
    bool followPlayhead = true;

    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::OwnedArray<juce::TextButton> altButtons;
    juce::Rectangle<float> bubble;
};

// ------------------------------------------------------------------ drag handle

/** Small icon you can drag (the MIDI file) straight into an Ableton track. */
class DragHandle : public ThemedView
{
public:
    std::function<juce::File()> createFile;
    juce::String label { "DRAG\nMIDI" };

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    bool dragging = false;
};

} // namespace typebeats
