#pragma once

#include "Theme.h"

class CortexAudioProcessor;

namespace cortex::ui
{
// The Capture button and the chip for the file it made. Cortex always keeps
// the last Capture Length seconds of its output; the button writes them to a
// WAV in the captures folder, and the chip can be dragged straight into the
// DAW (or clicked to show the file).
class CaptureStrip final : public juce::Component
{
public:
    explicit CaptureStrip (CortexAudioProcessor&);

    // Pulls the capture length into the button text (editor timer).
    void refresh();

    void resized() override;

    // Where captures go: the user's music folder, "Cortex Captures".
    static juce::File capturesFolder();

    // Last file written, for tests. Invalid before the first capture.
    juce::File getLastCapture() const { return lastCapture; }

    // What the button does; public so a test can run it without a click.
    void capture();

private:
    // The file chip: drag it to the DAW, click to reveal it.
    class Chip final : public juce::Component,
                       public juce::SettableTooltipClient
    {
    public:
        void setFile (const juce::File&, double seconds);
        void paint (juce::Graphics&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        juce::File file;
        juce::String text;
        bool dragging = false;
    };

    CortexAudioProcessor& processor;
    juce::TextButton button;
    Chip chip;
    juce::Label message;
    juce::File lastCapture;
    int shownSeconds = -1;
};
} // namespace cortex::ui
