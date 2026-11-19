#pragma once

#include "Widgets.h"

namespace quasar::ui
{

/** Base for the pages: transparent, covers the whole editor, draws its glass panels in paintPanels(). */
class Page : public juce::Component, protected juce::Timer
{
public:
    explicit Page (Processor& p) : proc (p) { setInterceptsMouseClicks (false, true); }
    /** Pages that handle clicks themselves (lists, rows) declare where: only there they take the mouse. */
    void setClickAreas (std::vector<juce::Rectangle<int>> areas)
    {
        clickAreas = std::move (areas);
        setInterceptsMouseClicks (! clickAreas.empty(), true);
    }
    bool hitTest (int x, int y) override
    {
        // the page itself only takes clicks in its areas; everywhere else only its visible controls do,
        // so pages that overlap (keyboard, voicing...) keep working
        for (auto& a : clickAreas) if (a.contains (x, y)) return true;
        for (int i = getNumChildComponents(); --i >= 0;)
        {
            auto* c = getChildComponent (i);
            if (c->isVisible() && c->getBounds().contains (x, y) && c->hitTest (x - c->getX(), y - c->getY())) return true;
        }
        return false;
    }
    ~Page() override { bindings.clear(); }
    void paint (juce::Graphics& g) override { paintPanels (g); }
    virtual void paintPanels (juce::Graphics&) {}
    std::function<void (const juce::String& page, int bus)> navigate;   // e.g. open the FX page of a bus

protected:
    void timerCallback() override { repaint(); }
    Knob& knob (const juce::String& id, const juce::String& label, juce::Point<int> centre, float diameter = 38.0f, float labelSize = 12.0f);
    Processor& proc;
    Bindings bindings;
    juce::OwnedArray<juce::Component> owned;
    std::vector<juce::Rectangle<int>> clickAreas;
    template <typename T, typename... A> T& make (A&&... a) { auto* c = new T (std::forward<A> (a)...); owned.add (c); addAndMakeVisible (c); return *c; }
};

/** Power dot in module headers. */
class PowerButton : public juce::Component
{
public:
    explicit PowerButton (juce::RangedAudioParameter&);
    ~PowerButton() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> att;
    bool on = false;
};

class OscPage : public Page
{
public:
    explicit OscPage (Processor&);
    void paintPanels (juce::Graphics&) override;
};

class BottomPanels : public Page
{
public:
    explicit BottomPanels (Processor&);
    void paintPanels (juce::Graphics&) override;
    void showEnv (int);
    void showLfo (int);
private:
    void timerCallback() override;
    EnvView* envView = nullptr; LfoView* lfoView = nullptr;
    juce::OwnedArray<ModSourceTab> envTabs, lfoTabs, macroLabels;
    std::array<std::array<Knob*, 5>, numEnvs> envKnobs {};
    std::array<std::array<Knob*, 5>, numLfos> lfoKnobs {};
    std::array<std::array<juce::Component*, 5>, numLfos> lfoButtons {};
    int env = 0, lfo = 0;
};

class VoicingPanel : public Page
{
public:
    explicit VoicingPanel (Processor&);
    void paintPanels (juce::Graphics&) override;
private:
    void timerCallback() override;
    juce::OwnedArray<juce::TextButton> modes;
};

class MixPage : public Page
{
public:
    explicit MixPage (Processor&);
    void paintPanels (juce::Graphics&) override;
private:
    void timerCallback() override;
    juce::OwnedArray<juce::TextButton> insertButtons;
};

/** One effect slot shown as a module on the FX page. */
class FxModule : public juce::Component, private juce::Timer
{
public:
    FxModule (Processor&, int bus, int slot);
    ~FxModule() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void rebuild();
    int bus, slot, type = -1;
private:
    void timerCallback() override;
    void drawIcon (juce::Graphics&, juce::Rectangle<float>);
    void drawDisplay (juce::Graphics&, juce::Rectangle<float>);
    Processor& proc;
    Bindings bindings;
    juce::OwnedArray<juce::Component> controls;
    std::unique_ptr<Switch> power;
    std::unique_ptr<Fader> fader;
    juce::TextButton up { "^" }, down { "v" };
};

class FxPage : public Page
{
public:
    explicit FxPage (Processor&);
    ~FxPage() override;
    void paintPanels (juce::Graphics&) override;
    void resized() override;
    void setBus (int);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void layoutModules();
    void showAddMenu (int slot);
    juce::Rectangle<int> rowArea (int i) const { return { 30, 222 + i * 48, 394, 40 }; }
    int count() const;
    TextTabs busTabs { { "MAIN", "BUS 1", "BUS 2" } };
    juce::TextButton addButton { "+ FX" }, chainButton { "Custom" };
    juce::Viewport viewport;
    juce::Component holder;
    juce::OwnedArray<FxModule> modules;
    juce::OwnedArray<Switch> rowSwitches;
    int bus = 0, dragRow = -1, dropRow = -1, lastSignature = -1;
};

class MatrixPage : public Page
{
public:
    explicit MatrixPage (Processor&);
    ~MatrixPage() override;
    void paintPanels (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    juce::Rectangle<float> rowRect (int visibleRow) const { return { 26.0f, 172.0f + (float) visibleRow * 37.6f, 940.0f, 33.0f }; }
    int rowAt (juce::Point<float>) const;
    float get (int slot, const char* w) const;
    void set (int slot, const char* w, float v);
    void sourceMenu (int slot, bool aux, juce::Rectangle<int>);
    void destMenu (int slot, juce::Rectangle<int>);
    juce::TextButton addButton { "+ ADD" }, clearButton { "CLEAR" };
    int scroll = 0, selected = 0, dragSlot = -1; float dragStart = 0; int dragColumn = 0;
};

class GlobalPage : public Page
{
public:
    explicit GlobalPage (Processor&);
    void paintPanels (juce::Graphics&) override;
private:
    void timerCallback() override;
    juce::OwnedArray<juce::TextButton> qualityButtons, osButtons;
    Meter* meter = nullptr;
};

/** SIMPLE: the quasar, the preset, the 8 macros (Analog Lab style). */
class SimplePage : public Page
{
public:
    explicit SimplePage (Processor&);
    void paintPanels (juce::Graphics&) override;
    std::function<void (int)> stepPreset;
private:
    void timerCallback() override;
    float spin = 0, energy = 0;
    juce::TextButton prev { "<" }, next { ">" };
};

/** ARP switch, the popup with the arp settings and the two wheels. */
class ArpPanel : public Page
{
public:
    explicit ArpPanel (Processor&);
    void paintPanels (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void openSettings();
};

} // namespace quasar::ui
