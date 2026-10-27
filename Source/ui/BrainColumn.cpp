#include "BrainColumn.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "Tooltips.h"

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
      settings (p.apvts, settingsFor (brainIndex), juce::String (colours::sourceName (cortex::dsp::brain1 + brainIndex)) + " ", 66,
                colours::forSource (cortex::dsp::brain1 + brainIndex))
{
    map.setAccent (colours::forSource (source));
    sounds.setAccent (colours::forSource (source));
    title.setText (fonts::titleCase (colours::sourceName (source)), juce::dontSendNotification);
    title.setFont (fonts::title (13.0f));
    title.setColour (juce::Label::textColourId, colours::forSource (source));
    addAndMakeVisible (title);

    status.setFont (fonts::ui (11.5f));
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

    status.setTooltip (tips::status);
    loadButton.setTooltip (tips::loadButton);
    clearButton.setTooltip (tips::clearButton);
    map.setTooltip (tips::brainMap);
    sounds.setTooltip (tips::soundList);
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
    // A flat region with a hairline edge and the source's colour as a rule
    // along the top.
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::background);
    g.fillRect (r);
    g.setColour (colours::outline);
    g.drawRect (r, 1.0f);
    g.setColour (colours::forSource (source));
    g.fillRect (r.removeFromTop (3.0f));
}

void BrainColumn::paintOverChildren (juce::Graphics& g)
{
    if (! dropHighlight)
        return;
    const auto colour = colours::forSource (source);
    g.setColour (colour.withAlpha (0.12f));
    g.fillRect (getLocalBounds());
    g.setColour (colour);
    g.drawRect (getLocalBounds().reduced (1), 3);
    g.setFont (fonts::title (16.0f));
    g.drawText (fonts::titleCase (juce::String ("Drop to build ") + colours::sourceName (source)), getLocalBounds(), juce::Justification::centred);
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
