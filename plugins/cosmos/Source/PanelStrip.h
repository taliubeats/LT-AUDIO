#pragma once

#include "Visuals.h"

namespace cosmos
{

/**
    The row (or grid, when the window is tall) of visual panels.
    - drag a divider to resize neighbours
    - hover a panel: its name opens a menu to swap the visual, arrows move it, x removes it
    - double-click: panel action (loudness / levels reset)
*/
class PanelStrip : public juce::Component
{
public:
    struct Panel
    {
        VisualType type;
        float weight = 1.0f;
        std::unique_ptr<Visual> visual;
    };

    PanelStrip();

    void setContext (const Analyzer*, const Readings*, const Skin*, float target, double time);

    void addPanel (VisualType, int index = -1);
    void removePanel (int index);
    void setType (int index, VisualType);
    int getNumPanels() const { return (int) panels.size(); }

    /** "type:weight,type:weight,..." */
    juce::String getLayout() const;
    void setLayout (const juce::String&);
    static juce::String defaultLayout();

    std::function<void()> onLayoutChanged;
    std::function<void()> onResetLoudness;
    bool forceGrid = false;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    /** Bounds of each panel for the current size (rows when tall). */
    std::vector<juce::Rectangle<float>> computeRects() const;

private:
    struct Header { juce::Rectangle<float> name, left, right, close; };
    Header headerFor (juce::Rectangle<float> panel) const;
    int panelAt (juce::Point<float>) const;
    int dividerAt (juce::Point<float>) const;   // index of the panel left of the divider, or -1
    void showTypeMenu (int index);
    void changed() { if (onLayoutChanged) onLayoutChanged(); }

    std::vector<Panel> panels;
    std::vector<std::vector<int>> rowsCache;
    const Analyzer* analyzer = nullptr;
    const Readings* readings = nullptr;
    const Skin* skin = nullptr;
    float target = -14.0f;
    double time = 0;
    int hover = -1, dragDivider = -1;
    float dragStartX = 0, dragStartLeft = 0, dragStartRight = 0;

    std::vector<std::vector<int>> rows() const;
};

} // namespace cosmos
