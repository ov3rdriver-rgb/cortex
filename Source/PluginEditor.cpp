#include "PluginEditor.h"
#include "Parameters.h"
#include "state/Settings.h"

CortexAudioProcessorEditor::CortexAudioProcessorEditor (CortexAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setResizable (true, true);
    setResizeLimits (1120, 760, 2600, 1800);

    // The theme: restyle and rebuild the view whenever it changes (the
    // attachment also delivers the current value, which builds the first one).
    auto& themeParam = *p.apvts.getParameter (cortex::ParamIDs::theme);
    themeAttachment = std::make_unique<juce::ParameterAttachment> (themeParam, [this, &themeParam] (float)
    {
        const int index = juce::roundToInt (themeParam.convertFrom0to1 (themeParam.getValue()));
        cortex::ui::applyStyle (cortex::ui::styleForTheme (index));
        cortex::settings::saveTheme (index);
        buildView();
    });
    themeAttachment->sendInitialUpdate();
    if (view == nullptr)
        buildView();

    setSize (1280, 820);
    startTimerHz (30);
}

CortexAudioProcessorEditor::~CortexAudioProcessorEditor()
{
    themeAttachment.reset();
    view.reset();
    setLookAndFeel (nullptr);
}

void CortexAudioProcessorEditor::buildView()
{
    view.reset();
    lookAndFeel.applyColours();
    setLookAndFeel (&lookAndFeel);
    view = std::make_unique<cortex::ui::MainView> (processor);
    addAndMakeVisible (*view);
    view->sendLookAndFeelChange();
    resized();
}

void CortexAudioProcessorEditor::resized()
{
    if (view != nullptr)
        view->setBounds (getLocalBounds());
}

void CortexAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    processor.loadBrainFrom (files, 0);
}

void CortexAudioProcessorEditor::timerCallback()
{
    if (view != nullptr)
        view->refresh();
}
