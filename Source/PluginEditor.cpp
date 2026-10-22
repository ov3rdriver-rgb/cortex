#include "PluginEditor.h"

namespace
{
    constexpr int brainPanelHeight = 76;
}

CortexAudioProcessorEditor::CortexAudioProcessorEditor (CortexAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), generic (p)
{
    brainLabel.setJustificationType (juce::Justification::centredLeft);
    brainLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (brainLabel);
    addAndMakeVisible (progressBar);

    chooseButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose samples or a folder for the brain");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  juce::StringArray paths;
                                  for (const auto& f : fc.getResults())
                                      paths.add (f.getFullPathName());
                                  if (! paths.isEmpty())
                                      processor.loadBrainFrom (paths);
                              });
    };
    rebuildButton.onClick = [this] { processor.rebuildBrain(); };
    clearButton.onClick = [this] { processor.clearBrain(); };
    for (auto* b : { &chooseButton, &rebuildButton, &clearButton })
        addAndMakeVisible (*b);

    addAndMakeVisible (generic);

    credits.setText ("Cortex - AGPL-3.0. Brain engine from Samplebrain (Then Try This / Aphex Twin). "
                     "Cepstral morph concept: Alexander Panos.",
                     juce::dontSendNotification);
    credits.setFont (juce::FontOptions (12.0f));
    credits.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (credits);

    setResizable (true, true);
    setSize (560, 720);
    timerCallback();
    startTimerHz (10);
}

juce::Rectangle<int> CortexAudioProcessorEditor::brainArea() const
{
    return getLocalBounds().removeFromTop (brainPanelHeight).reduced (8, 6);
}

void CortexAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    const auto area = brainArea().toFloat();
    g.setColour (juce::Colours::white.withAlpha (dragHighlight ? 0.18f : 0.06f));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (dragHighlight ? 0.8f : 0.2f));
    g.drawRoundedRectangle (area, 6.0f, dragHighlight ? 2.0f : 1.0f);
}

void CortexAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto brain = area.removeFromTop (brainPanelHeight).reduced (16, 12);

    auto buttons = brain.removeFromRight (230);
    chooseButton.setBounds (buttons.removeFromLeft (74).reduced (2, 8));
    rebuildButton.setBounds (buttons.removeFromLeft (80).reduced (2, 8));
    clearButton.setBounds (buttons.reduced (2, 8));

    brainLabel.setBounds (brain.removeFromTop (brain.getHeight() / 2));
    progressBar.setBounds (brain.reduced (0, 3));

    credits.setBounds (area.removeFromBottom (28));
    generic.setBounds (area);
}

void CortexAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    setDragHighlight (false);
    processor.loadBrainFrom (files);
}

void CortexAudioProcessorEditor::setDragHighlight (bool shouldHighlight)
{
    dragHighlight = shouldHighlight;
    repaint (brainArea());
}

void CortexAudioProcessorEditor::timerCallback()
{
    const auto s = processor.getBrainStatus();

    juce::String text;
    if (s.building)
        text = "Building brain: " + s.message;
    else if (s.numBlocks > 0)
        text = "Brain: " + juce::String (s.numBlocks) + " blocks, " + juce::String (s.numSounds) + " sounds"
             + (s.playingBlock >= 0 ? "  |  playing block " + juce::String (s.playingBlock) : juce::String());
    else
        text = "Drop a folder of samples here to build a brain";

    brainLabel.setText (text, juce::dontSendNotification);
    progress = s.building ? (double) s.progress : (s.numBlocks > 0 ? 1.0 : 0.0);
    rebuildButton.setEnabled (! processor.getBrainFiles().isEmpty());
    clearButton.setEnabled (s.numBlocks > 0 || s.building);
}
