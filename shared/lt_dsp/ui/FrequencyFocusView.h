#pragma once

namespace lt::ui
{

/**
    The "Frequency Focus" window (design/pulsar/eq-*.webp) shared by LT PULSAR and LT KNOCK.
    Real-time spectrum 20 Hz - 20 kHz, draggable LOW CUT / HIGH CUT, FULL / LOW / MID / HIGH,
    INVERT and SOLO. The focused band glows (nebula + particles); the rest stays dim.
    Fills the whole editor; the plugin shows/hides it with its EQ button.
*/
class FrequencyFocusView : public juce::Component,
                           private juce::Timer
{
public:
    FrequencyFocusView (juce::AudioProcessorValueTreeState&, dsp::SpectrumAnalyser&, const juce::String& productId);
    ~FrequencyFocusView() override;

    std::function<void()> onClose;

    /** Glass style (LT KNOCK): a translucent panel over the plugin's nebula, outlined spectrum, pill buttons.
        Default is the "hardware" style of LT PULSAR. */
    void setGlassStyle (bool);

    void setPalette (const Palette&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    /** Frequency <-> x inside the graph (log scale). */
    float xForHz (float hz) const noexcept;
    float hzForX (float x) const noexcept;

private:
    enum class Drag { none, low, high, both };

    void timerCallback() override;
    dsp::FocusSettings current() const;
    void setBand (int band);
    void setCutoffs (float lo, float hi);
    Drag dragTargetAt (juce::Point<float>) const;
    juce::Path spectrumPath (juce::Rectangle<float> graph) const;
    void drawGlowBlobs (juce::Graphics&, juce::Rectangle<float> region, float energy);
    void drawHandle (juce::Graphics&, float x, const juce::String& title, float hz, bool hot);
    void paintGlass (juce::Graphics&);
    bool glassStyle = false;

    juce::AudioProcessorValueTreeState& state;
    dsp::SpectrumAnalyser& analyser;
    juce::String productId;
    Palette palette = Palette::dark();

    /** Rectangular "hardware" buttons and switches like the reference design. */
    struct FocusLookAndFeel : public GlassLookAndFeel
    {
        bool glass = false;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hover, bool down) override;
    };
    FocusLookAndFeel lnf;

    juce::RangedAudioParameter *bandParam, *lowParam, *highParam;
    juce::TextButton bandButtons[4] { juce::TextButton ("FULL"), juce::TextButton ("LOW"), juce::TextButton ("MID"), juce::TextButton ("HIGH") };
    juce::ToggleButton invertButton { "INVERT" }, soloButton { "SOLO" };
    juce::TextButton backButton { "BACK" };
    std::unique_ptr<juce::ButtonParameterAttachment> invertAttachment, soloAttachment;

    juce::Rectangle<float> panel, graph, bottomBar;
    struct Spark { float x, y, vx, vy, life, size; bool white; };
    std::vector<Spark> sparks;
    juce::Random random;
    Drag dragging = Drag::none, hover = Drag::none;
    float dragStartLo = 0, dragStartHi = 0, dragStartX = 0;
    double time = 0;
};

} // namespace lt::ui
