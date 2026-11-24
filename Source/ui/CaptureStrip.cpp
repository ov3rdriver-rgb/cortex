#include "CaptureStrip.h"
#include "PluginProcessor.h"
#include "Tooltips.h"

namespace cortex::ui
{
CaptureStrip::CaptureStrip (CortexAudioProcessor& p) : processor (p)
{
    button.onClick = [this] { capture(); };
    addAndMakeVisible (button);
    addChildComponent (chip);

    message.setFont (fonts::ui (10.5f));
    message.setColour (juce::Label::textColourId, colours::dim);
    message.setJustificationType (juce::Justification::centredRight);
    addChildComponent (message);

    button.setTooltip (tips::captureButton);
    chip.setTooltip (tips::captureChip);
    refresh();
}

juce::File CaptureStrip::capturesFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Cortex Captures");
}

void CaptureStrip::refresh()
{
    const int seconds = processor.getCaptureSeconds();
    if (seconds != shownSeconds)
    {
        shownSeconds = seconds;
        button.setButtonText ("Capture " + juce::String (seconds) + " s");
    }
}

void CaptureStrip::capture()
{
    const auto name = "Cortex " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M.%S") + ".wav";
    const auto file = capturesFolder().getChildFile (name);

    juce::String error;
    const double seconds = processor.saveCapture (file, error);
    if (seconds <= 0.0)
    {
        chip.setVisible (false);
        message.setText (error, juce::dontSendNotification);
        message.setVisible (true);
        return;
    }

    lastCapture = file;
    message.setVisible (false);
    chip.setFile (file, seconds);
    chip.setVisible (true);
    resized();
}

void CaptureStrip::resized()
{
    auto area = getLocalBounds();
    button.setBounds (area.removeFromRight (110).reduced (2, 3));
    area.removeFromRight (6);
    chip.setBounds (area.reduced (0, 3));
    message.setBounds (area);
}

//==============================================================================
void CaptureStrip::Chip::setFile (const juce::File& f, double seconds)
{
    file = f;
    text = f.getFileNameWithoutExtension() + "  " + juce::String (seconds, 0) + " s";
    repaint();
}

void CaptureStrip::Chip::paint (juce::Graphics& g)
{
    // The file as a cream tag ruled in grease pencil (ink while it is being dragged).
    drawTag (g, getLocalBounds().toFloat(), text, 10.5f, dragging ? colours::ink : colours::now, juce::Justification::centredLeft);
    g.setColour (colours::ink.withAlpha (0.6f));
    g.setFont (fonts::label (8.5f));
    g.drawText (fonts::titleCase ("drag to DAW"), getLocalBounds().reduced (8, 0), juce::Justification::centredRight, true);
}

void CaptureStrip::Chip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! file.existsAsFile() || e.getDistanceFromDragStart() < 6)
        return;
    dragging = true;
    repaint();
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                [this] { dragging = false; repaint(); });
}

void CaptureStrip::Chip::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging && e.mouseWasClicked() && file.existsAsFile())
        file.revealToUser();
}
} // namespace cortex::ui
