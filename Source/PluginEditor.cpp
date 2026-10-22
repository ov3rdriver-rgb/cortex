#include "PluginEditor.h"

CortexAudioProcessorEditor::CortexAudioProcessorEditor (CortexAudioProcessor& p)
    : AudioProcessorEditor (&p), generic (p)
{
    addAndMakeVisible (generic);

    credits.setText ("Cortex - AGPL-3.0. Brain engine from Samplebrain (Then Try This / Aphex Twin). "
                     "Cepstral morph concept: Alexander Panos.",
                     juce::dontSendNotification);
    credits.setFont (juce::FontOptions (12.0f));
    credits.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (credits);

    setResizable (true, true);
    setSize (520, 640);
}

void CortexAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void CortexAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    credits.setBounds (area.removeFromBottom (28));
    generic.setBounds (area);
}
