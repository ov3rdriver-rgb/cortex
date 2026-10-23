#include "PluginEditor.h"
#include "Parameters.h"
#include "Version.h"

namespace
{
    constexpr int headerHeight = 74;
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

    for (auto* l : { &brainLabel, &brain2Label })
    {
        l->setFont (juce::FontOptions (13.0f));
        l->setColour (juce::Label::textColourId, colours::text);
        addAndMakeVisible (*l);
    }
    addAndMakeVisible (progressBar);

    chooseButton.onClick = [this] { chooseBrainFiles (0); };
    choose2Button.onClick = [this] { chooseBrainFiles (1); };
    rebuildButton.onClick = [this]
    {
        for (int b = 0; b < CortexAudioProcessor::numBrains; ++b)
            if (! processor.getBrainFiles (b).isEmpty())
                processor.rebuildBrain (b);
    };
    clearButton.onClick = [this] { processor.clearBrain (0); };
    clear2Button.onClick = [this] { processor.clearBrain (1); };
    for (auto* b : { &chooseButton, &rebuildButton, &clearButton, &choose2Button, &clear2Button })
        addAndMakeVisible (*b);

    viewBrain.addItemList ({ "Brain 1", "Brain 2" }, 1);
    viewBrain.setTooltip ("Which brain the map and sound list show, and which one dropped files load into");
    viewBrain.setSelectedId (1, juce::dontSendNotification);
    viewBrain.onChange = [this] { timerCallback(); };
    addAndMakeVisible (viewBrain);

    addAndMakeVisible (xyPad);
    addAndMakeVisible (cepstrumView);
    addAndMakeVisible (brainMap);
    addAndMakeVisible (soundList);

    // Parameter tabs. Every parameter lives on exactly one tab.
    const auto tabColour = colours::panel;
    juce::StringArray morphIds { envX, envY, detailX, detailY, linkTriangles, phaseMode, fftSize, lowLatency };
    if (! p.isInstrument())
        morphIds.insert (0, inputSource);
    tabs.addTab ("Morph", tabColour, new cortex::ui::ParamPanel (p.apvts, morphIds), true);
    tabs.addTab ("Sources", tabColour, new cortex::ui::ParamPanel (p.apvts, { cutoffIn, cutoffB1, cutoffB2,
                                                                              formantIn, formantB1, formantB2 }), true);
    tabs.addTab ("Mod", tabColour, new cortex::ui::ParamPanel (p.apvts, { lfoRate, lfoDepth, lfoShape, lfoTarget,
                                                                          envDepth, envAttack, envRelease }), true);
    tabs.addTab ("Freeze", tabColour, new cortex::ui::ParamPanel (p.apvts, { freezeEnvIn, freezeDetIn, freezeEnvB1, freezeDetB1,
                                                                             freezeEnvB2, freezeDetB2 }), true);
    tabs.addTab ("Brain", tabColour, new cortex::ui::ParamPanel (p.apvts, { chaos, memory, cepstralGlide, matchTimbre, matchPitch, envTransplant, searchMode,
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

    auto row1 = header.removeFromTop (header.getHeight() / 2);
    auto buttons1 = row1.removeFromRight (250);
    chooseButton.setBounds (buttons1.removeFromLeft (80).reduced (3, 4));
    rebuildButton.setBounds (buttons1.removeFromLeft (85).reduced (3, 4));
    clearButton.setBounds (buttons1.reduced (3, 4));
    row1.reduce (12, 0);
    progressBar.setBounds (row1.removeFromBottom (6));
    brainLabel.setBounds (row1);

    auto buttons2 = header.removeFromRight (250);
    choose2Button.setBounds (buttons2.removeFromLeft (80).reduced (3, 4));
    viewBrain.setBounds (buttons2.removeFromLeft (85).reduced (3, 4));
    clear2Button.setBounds (buttons2.reduced (3, 4));
    brain2Label.setBounds (header.reduced (12, 0));

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
    processor.loadBrainFrom (files, viewBrain.getSelectedId() - 1);
}

void CortexAudioProcessorEditor::chooseBrainFiles (int brain)
{
    chooser = std::make_unique<juce::FileChooser> (brain == 0 ? "Choose samples or a folder for Brain 1"
                                                              : "Choose samples or a folder for Brain 2");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this, brain] (const juce::FileChooser& fc)
                          {
                              juce::StringArray paths;
                              for (const auto& f : fc.getResults())
                                  paths.add (f.getFullPathName());
                              if (! paths.isEmpty())
                                  processor.loadBrainFrom (paths, brain);
                          });
}

juce::String CortexAudioProcessorEditor::statusText (int brain) const
{
    const auto s = processor.getBrainStatus (brain);
    const juce::String name = brain == 0 ? "Brain 1" : "Brain 2";
    if (s.building)
        return name + ": building - " + s.message;
    if (s.numBlocks > 0)
        return name + ": " + juce::String (s.numBlocks) + " blocks from " + juce::String (s.numSounds) + " sounds";
    return brain == 0 ? "Brain 1: drop a folder of samples anywhere on this window"
                      : "Brain 2: drop or load a second folder to morph between two brains";
}

void CortexAudioProcessorEditor::setDragHighlight (bool shouldHighlight)
{
    dragHighlight = shouldHighlight;
    repaint();
}

void CortexAudioProcessorEditor::timerCallback()
{
    const auto s1 = processor.getBrainStatus (0);
    const auto s2 = processor.getBrainStatus (1);
    brainLabel.setText (statusText (0), juce::dontSendNotification);
    brain2Label.setText (statusText (1), juce::dontSendNotification);
    brain2Label.setColour (juce::Label::textColourId, s2.numBlocks > 0 || s2.building ? cortex::ui::colours::text
                                                                                       : cortex::ui::colours::dim);

    const bool building = s1.building || s2.building;
    progress = s1.building ? (double) s1.progress : (s2.building ? (double) s2.progress : 0.0);
    progressBar.setVisible (building);
    rebuildButton.setEnabled (! processor.getBrainFiles (0).isEmpty() || ! processor.getBrainFiles (1).isEmpty());
    clearButton.setEnabled (s1.numBlocks > 0 || s1.building);
    clear2Button.setEnabled (s2.numBlocks > 0 || s2.building);

    const int shown = viewBrain.getSelectedId() - 1;
    const auto brain = processor.getBrain (shown);
    brainMap.setBrain (brain);
    soundList.setBrain (brain);
    brainMap.setPlayingBlock ((shown == 0 ? s1 : s2).playingBlock);

    cortex::dsp::MorphSnapshot snapshot;
    if (processor.readMorphSnapshot (snapshot))
    {
        xyPad.setSnapshot (snapshot);
        cepstrumView.setSnapshot (snapshot);
    }
}
