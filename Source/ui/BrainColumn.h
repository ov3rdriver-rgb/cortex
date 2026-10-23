#pragma once

#include "BrainMap.h"
#include "ParamPanel.h"
#include "Theme.h"

class CortexAudioProcessor;

namespace cortex::ui
{
// Everything about one brain: title, status, Load / Clear, build progress,
// the block map, the sound list, and that brain's own morph settings
// (cutoff, formant, freeze). Files dropped on the column load into it.
class BrainColumn final : public juce::Component,
                          public juce::FileDragAndDropTarget
{
public:
    BrainColumn (CortexAudioProcessor&, int brainIndex);

    // Pulls status, brain and playing block from the processor (editor timer).
    void refresh();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHighlight = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHighlight = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void chooseFiles();

    CortexAudioProcessor& processor;
    const int brain;
    const int source;   // dsp::Source of this brain

    juce::Label title, status;
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };
    juce::TextButton loadButton { "Load..." }, clearButton { "Clear" };
    std::unique_ptr<juce::FileChooser> chooser;
    BrainMap map;
    SoundList sounds;
    ParamPanel settings;
    bool dropHighlight = false;
};
} // namespace cortex::ui
