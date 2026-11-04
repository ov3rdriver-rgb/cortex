#pragma once

#include "Theme.h"

namespace cortex::ui
{
// The explainer behind the "?" in the header: what Cortex does, what the
// cepstral morph is doing, and how to get a first sound. A scrim over the
// whole view with a scrolling page in the middle; click outside the page,
// Close, or Escape to put it away.
class HelpOverlay final : public juce::Component
{
public:
    explicit HelpOverlay (bool instrument);

    void show();
    void hide();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // The text, as shown (for tests).
    static juce::String plainText (bool instrument);

private:
    // The page: the sections laid out as text, as tall as they need to be at
    // the width it is given.
    class Page final : public juce::Component
    {
    public:
        explicit Page (bool instrument);
        void setPageWidth (int width);
        void paint (juce::Graphics&) override;

    private:
        juce::AttributedString text;
        juce::TextLayout layout;
    };

    juce::Label heading;
    juce::TextButton closeButton { "Close" };
    juce::Viewport viewport;
    Page page;
    juce::Rectangle<int> panel;
};
} // namespace cortex::ui
