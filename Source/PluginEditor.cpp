#include "PluginEditor.h"
#include "Parameters.h"
#include "Version.h"

namespace
{
    constexpr int headerHeight = 54;
    constexpr int footerHeight = 24;
    constexpr int gap = 8;
}

CortexAudioProcessorEditor::CortexAudioProcessorEditor (CortexAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      xyPad (p.apvts), cepstrumView (p.apvts)
{
    setLookAndFeel (&lookAndFeel);
    using namespace cortex::ParamIDs;
    namespace colours = cortex::ui::colours;

    title.setText (p.isInstrument() ? "CORTEX INSTRUMENT" : "CORTEX", juce::dontSendNotification);
    title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    brainLabel.setFont (juce::FontOptions (13.0f));
    brainLabel.setColour (juce::Label::textColourId, colours::text);
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

    addAndMakeVisible (xyPad);
    addAndMakeVisible (cepstrumView);
    addAndMakeVisible (brainMap);
    addAndMakeVisible (soundList);

    // Parameter tabs. Every parameter lives on exactly one tab.
    const auto tabColour = colours::panel;
    juce::StringArray morphIds { phaseMode, fftSize, cutoffA, cutoffB, cornerGainTL, cornerGainBR };
    if (! p.isInstrument())
        morphIds.insert (0, bSource);
    tabs.addTab ("Morph", tabColour, new cortex::ui::ParamPanel (p.apvts, morphIds), true);
    tabs.addTab ("Brain", tabColour, new cortex::ui::ParamPanel (p.apvts, { matchTimbre, matchPitch, envTransplant, searchMode,
                                                                            novelty, boredom, stickyness, blockSize, overlap }), true);
    if (p.isInstrument())
        tabs.addTab ("Synth", tabColour, new cortex::ui::ParamPanel (p.apvts, { oscWave, oscGlide, ampAttack, ampRelease }), true);
    tabs.addTab ("Output", tabColour, new cortex::ui::ParamPanel (p.apvts, { dryWet, outGain, limiter, gateOn, gateThresh,
                                                                             autoMute, autoMuteMs }), true);
    tabs.setTabBarDepth (26);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    credits.setText ("AGPL-3.0. Brain engine from Samplebrain (Then Try This / Aphex Twin). Cepstral morph concept: Alexander Panos.",
                     juce::dontSendNotification);
    credits.setFont (juce::FontOptions (11.0f));
    credits.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (credits);

    versionLabel.setText ("v" CORTEX_VERSION_STRING " (" CORTEX_GIT_HASH ")", juce::dontSendNotification);
    versionLabel.setFont (juce::FontOptions (11.0f));
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (versionLabel);

    setResizable (true, true);
    setResizeLimits (900, 620, 2400, 1600);
    setSize (1040, 700);
    timerCallback();
    startTimerHz (30);
}

CortexAudioProcessorEditor::~CortexAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CortexAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (cortex::ui::colours::background);
}

void CortexAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (! dragHighlight)
        return;
    g.setColour (cortex::ui::colours::b.withAlpha (0.12f));
    g.fillRect (getLocalBounds());
    g.setColour (cortex::ui::colours::b);
    g.drawRect (getLocalBounds(), 3);
    g.setFont (juce::FontOptions (22.0f));
    g.drawText ("Drop to build a brain", getLocalBounds(), juce::Justification::centred);
}

void CortexAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (gap);

    auto header = area.removeFromTop (headerHeight - gap);
    title.setBounds (header.removeFromLeft (220));
    auto buttons = header.removeFromRight (250);
    chooseButton.setBounds (buttons.removeFromLeft (80).reduced (3, 8));
    rebuildButton.setBounds (buttons.removeFromLeft (85).reduced (3, 8));
    clearButton.setBounds (buttons.reduced (3, 8));
    header.reduce (12, 4);
    brainLabel.setBounds (header.removeFromTop (header.getHeight() / 2));
    progressBar.setBounds (header.reduced (0, 3));

    auto footer = area.removeFromBottom (footerHeight);
    versionLabel.setBounds (footer.removeFromRight (140));
    credits.setBounds (footer);
    area.removeFromBottom (gap / 2);

    // Left column: square XY pad, cepstrum view below it.
    const int cepstrumHeight = 150;
    const int padSize = juce::jmin (area.getHeight() - cepstrumHeight - gap, area.getWidth() / 2);
    auto left = area.removeFromLeft (padSize);
    xyPad.setBounds (left.removeFromTop (padSize));
    left.removeFromTop (gap);
    cepstrumView.setBounds (left);
    area.removeFromLeft (gap);

    // Right column: brain map + sound list, parameter tabs below.
    auto tabArea = area.removeFromBottom (juce::jmax (190, area.getHeight() * 2 / 5));
    tabs.setBounds (tabArea);
    area.removeFromBottom (gap);
    soundList.setBounds (area.removeFromRight (juce::jmin (240, area.getWidth() / 3)));
    area.removeFromRight (gap);
    brainMap.setBounds (area);
}

void CortexAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    setDragHighlight (false);
    processor.loadBrainFrom (files);
}

void CortexAudioProcessorEditor::setDragHighlight (bool shouldHighlight)
{
    dragHighlight = shouldHighlight;
    repaint();
}

void CortexAudioProcessorEditor::timerCallback()
{
    const auto s = processor.getBrainStatus();

    juce::String text;
    if (s.building)
        text = "Building brain: " + s.message;
    else if (s.numBlocks > 0)
        text = "Brain: " + juce::String (s.numBlocks) + " blocks from " + juce::String (s.numSounds) + " sounds";
    else
        text = "Drop a folder of samples anywhere on this window to build a brain";

    brainLabel.setText (text, juce::dontSendNotification);
    progress = s.building ? (double) s.progress : 0.0;
    progressBar.setVisible (s.building);
    rebuildButton.setEnabled (! processor.getBrainFiles().isEmpty());
    clearButton.setEnabled (s.numBlocks > 0 || s.building);

    const auto brain = processor.getBrain();
    brainMap.setBrain (brain);
    soundList.setBrain (brain);
    brainMap.setPlayingBlock (s.playingBlock);

    cortex::dsp::MorphSnapshot snapshot;
    if (processor.readMorphSnapshot (snapshot))
    {
        xyPad.setSnapshot (snapshot);
        cepstrumView.setSnapshot (snapshot);
    }
}
