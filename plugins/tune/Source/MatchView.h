#pragma once

#include "Views.h"
#include "MatchAnalysis.h"

namespace tune
{

/**
    MATCH REFERENCE window: drop a song (or a vocal stem), it is analysed in the background and the
    detected settings are listed with on/off switches. MATCH AMOUNT sets how much is copied.
*/
class MatchView : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  private juce::Timer
{
public:
    enum Row { keyRow = 0, retuneRow, humanizeRow, flexRow, vibratoRow, formantRow, rowCount };

    MatchView (const Palette&, std::shared_ptr<MatchResult> previous);
    ~MatchView() override;

    std::function<void (const MatchResult&, std::array<bool, rowCount> enabled, float amount)> onApply;
    std::function<void()> onClose;
    std::function<void (std::shared_ptr<MatchResult>)> onResult;   // to keep it in the processor

    void setPalette (const Palette&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override  { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override             { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

    void analyse (const juce::File&);

private:
    void timerCallback() override;
    void browse();
    void refreshRows();
    juce::Rectangle<float> dropArea() const   { return { 34.0f, 88.0f, 1032.0f, 172.0f }; }
    juce::Rectangle<float> previewArea() const { return { 558.0f, 278.0f, 508.0f, 240.0f }; }
    juce::Rectangle<float> rowArea (int i) const { return { 34.0f, 281.0f + 41.0f * (float) i, 508.0f, 30.0f }; }

    Palette palette;
    std::shared_ptr<MatchResult> result;
    juce::String status, analysingName;
    std::shared_ptr<std::atomic<bool>> cancel;
    bool analysing = false, dragOver = false;
    float time = 0.0f;

    GlassSwitch switches[rowCount];
    TuneKnob amount { "" };
    juce::TextButton applyButton { "APPLY TO LT TUNE" }, cancelButton { "CANCEL" };
    IconButton closeButton { IconButton::Icon::close };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatchView)
};

} // namespace tune
