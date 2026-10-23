#include "BrainColumn.h"
#include "Parameters.h"
#include "PluginProcessor.h"

namespace cortex::ui
{
namespace
{
    juce::StringArray settingsFor (int brain)
    {
        using namespace cortex::ParamIDs;
        if (brain == 0)
            return { cutoffB1, formantB1, freezeEnvB1, freezeDetB1 };
        return { cutoffB2, formantB2, freezeEnvB2, freezeDetB2 };
    }
}

BrainColumn::BrainColumn (CortexAudioProcessor& p, int brainIndex)
    : processor (p), brain (brainIndex), source (cortex::dsp::brain1 + brainIndex),
      settings (p.apvts, settingsFor (brainIndex), juce::String (colours::sourceName (cortex::dsp::brain1 + brainIndex)) + " ", 62)
{
    title.setText (colours::sourceName (source), juce::dontSendNotification);
    title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::forSource (source));
    addAndMakeVisible (title);

    status.setFont (juce::FontOptions (12.0f));
    status.setColour (juce::Label::textColourId, colours::dim);
    status.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (status);

    addChildComponent (progressBar);
    progressBar.setColour (juce::ProgressBar::foregroundColourId, colours::forSource (source));

    loadButton.onClick = [this] { chooseFiles(); };
    clearButton.onClick = [this] { processor.clearBrain (brain); };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);

    addAndMakeVisible (map);
    addAndMakeVisible (sounds);
    addAndMakeVisible (settings);
    refresh();
}

void BrainColumn::chooseFiles()
{
    chooser = std::make_unique<juce::FileChooser> (juce::String ("Choose samples or a folder for ") + colours::sourceName (source));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc)
                          {
                              juce::StringArray paths;
                              for (const auto& f : fc.getResults())
                                  paths.add (f.getFullPathName());
                              if (! paths.isEmpty())
                                  processor.loadBrainFrom (paths, brain);
                          });
}

void BrainColumn::filesDropped (const juce::StringArray& files, int, int)
{
    dropHighlight = false;
    repaint();
    processor.loadBrainFrom (files, brain);
}

void BrainColumn::refresh()
{
    const auto s = processor.getBrainStatus (brain);

    juce::String text;
    if (s.building)
        text = s.message;
    else if (s.numBlocks > 0)
        text = juce::String (s.numBlocks) + " blocks from " + juce::String (s.numSounds) + (s.numSounds == 1 ? " sound" : " sounds");
    else
        text = "Empty: drop a folder of samples here";
    status.setText (text, juce::dontSendNotification);

    progress = s.building ? (double) s.progress : 0.0;
    progressBar.setVisible (s.building);
    clearButton.setEnabled (s.numBlocks > 0 || s.building);

    const auto b = processor.getBrain (brain);
    map.setBrain (b);
    sounds.setBrain (b);
    map.setPlayingBlock (s.playingBlock);
}

void BrainColumn::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel.darker (0.15f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::forSource (source).withAlpha (0.5f));
    g.fillRoundedRectangle (r.removeFromTop (3.0f).reduced (8.0f, 0.0f), 1.5f);
}

void BrainColumn::paintOverChildren (juce::Graphics& g)
{
    if (! dropHighlight)
        return;
    const auto colour = colours::forSource (source);
    g.setColour (colour.withAlpha (0.12f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (colour);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 8.0f, 3.0f);
    g.setFont (juce::FontOptions (18.0f));
    g.drawText (juce::String ("Drop to build ") + colours::sourceName (source), getLocalBounds(), juce::Justification::centred);
}

void BrainColumn::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto header = area.removeFromTop (28);
    clearButton.setBounds (header.removeFromRight (64).reduced (2, 2));
    loadButton.setBounds (header.removeFromRight (74).reduced (2, 2));
    title.setBounds (header);

    auto statusRow = area.removeFromTop (22);
    progressBar.setBounds (statusRow.removeFromBottom (5));
    status.setBounds (statusRow);
    area.removeFromTop (4);

    settings.setBounds (area.removeFromBottom (110));
    area.removeFromBottom (6);
    sounds.setBounds (area.removeFromBottom (juce::jlimit (60, 110, area.getHeight() / 4)));
    area.removeFromBottom (6);
    map.setBounds (area);
}
} // namespace cortex::ui
