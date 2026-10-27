#include "PluginEditor.h"
#include "Parameters.h"
#include "Version.h"

namespace
{
    constexpr int headerHeight = 40;
    constexpr int footerHeight = 20;
    constexpr int bottomHeight = 190;
    constexpr int columnWidth = 300;
    constexpr int gap = 8;

    juce::StringArray inputIds (bool instrument)
    {
        using namespace cortex::ParamIDs;
        if (instrument)
            return { oscWave, oscGlide, ampAttack, ampRelease, cutoffIn, formantIn, freezeEnvIn, freezeDetIn };
        return { inputSource, cutoffIn, formantIn, freezeEnvIn, freezeDetIn, gateOn, gateThresh };
    }
}

CortexAudioProcessorEditor::CortexAudioProcessorEditor (CortexAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      brain1Column (p, 0), brain2Column (p, 1),
      envelopeTriangle (p.apvts, cortex::ParamIDs::envX, cortex::ParamIDs::envY, "Envelope (timbre)"),
      detailTriangle (p.apvts, cortex::ParamIDs::detailX, cortex::ParamIDs::detailY, "Detail (pitch, texture)"),
      cepstrum (p.apvts),
      inputPanel (p.apvts, inputIds (p.isInstrument()), "Input ", 74, cortex::ui::colours::input)
{
    setLookAndFeel (&lookAndFeel);
    using namespace cortex::ParamIDs;
    namespace colours = cortex::ui::colours;

    title.setText (p.isInstrument() ? "CORTEX INSTRUMENT" : "CORTEX", juce::dontSendNotification);
    title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    versionLabel.setText ("v" CORTEX_VERSION_STRING " (" CORTEX_GIT_HASH ")", juce::dontSendNotification);
    versionLabel.setFont (juce::FontOptions (11.0f));
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (versionLabel);

    rebuildButton.onClick = [this]
    {
        for (int b = 0; b < CortexAudioProcessor::numBrains; ++b)
            if (! processor.getBrainFiles (b).isEmpty())
                processor.rebuildBrain (b);
    };
    addAndMakeVisible (rebuildButton);
    addAndMakeVisible (meters);

    addAndMakeVisible (brain1Column);
    addAndMakeVisible (brain2Column);
    addAndMakeVisible (envelopeTriangle);
    addAndMakeVisible (detailTriangle);
    linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, linkTriangles, linkToggle);
    addAndMakeVisible (linkToggle);
    addAndMakeVisible (envelopes);
    addAndMakeVisible (cepstrum);

    inputTitle.setText (p.isInstrument() ? "Input: oscillator" : "Input: track or sidechain", juce::dontSendNotification);
    inputTitle.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    inputTitle.setColour (juce::Label::textColourId, colours::forSource (cortex::dsp::input));
    addAndMakeVisible (inputTitle);
    addAndMakeVisible (inputPanel);

    const auto tabColour = colours::panel;
    tabs.addTab ("Search", tabColour, new cortex::ui::ParamPanel (p.apvts, { chaos, memory, cepstralGlide, matchTimbre, matchPitch,
                                                                             envTransplant, searchMode, novelty, boredom, stickyness,
                                                                             blockSize, overlap }), true);
    tabs.addTab ("Mod", tabColour, new cortex::ui::ParamPanel (p.apvts, { lfoRate, lfoDepth, lfoShape, lfoTarget,
                                                                          envDepth, envAttack, envRelease }), true);
    tabs.addTab ("Output", tabColour, new cortex::ui::ParamPanel (p.apvts, { phaseMode, fftSize, lowLatency, dryWet, outGain,
                                                                             limiter, autoMute, autoMuteMs }), true);
    tabs.setTabBarDepth (26);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    credits.setText ("AGPL-3.0. Brain engine from Samplebrain (Then Try This / Aphex Twin). Cepstral morph concept: Alexander Panos.",
                     juce::dontSendNotification);
    credits.setFont (juce::FontOptions (11.0f));
    credits.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (credits);

    setResizable (true, true);
    setResizeLimits (1120, 760, 2600, 1800);
    setSize (1280, 820);
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

    // The Input panel's backdrop, tinted like the Input.
    auto input = inputTitle.getBounds().getUnion (inputPanel.getBounds()).expanded (8, 6).toFloat();
    g.setColour (cortex::ui::colours::panel.darker (0.15f));
    g.fillRoundedRectangle (input, 8.0f);
    g.setColour (cortex::ui::colours::forSource (cortex::dsp::input).withAlpha (0.5f));
    g.fillRoundedRectangle (input.removeFromTop (3.0f).reduced (8.0f, 0.0f), 1.5f);
}

void CortexAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (gap);

    auto header = area.removeFromTop (headerHeight - gap);
    title.setBounds (header.removeFromLeft (260));
    versionLabel.setBounds (header.removeFromRight (150));
    rebuildButton.setBounds (header.removeFromRight (130).reduced (2, 3));
    header.removeFromRight (gap);
    meters.setBounds (header.removeFromRight (juce::jmin (260, header.getWidth())));
    area.removeFromTop (gap);

    credits.setBounds (area.removeFromBottom (footerHeight));
    area.removeFromBottom (gap / 2);

    // Bottom row: Input panel | tabs.
    auto bottom = area.removeFromBottom (bottomHeight);
    area.removeFromBottom (gap);
    auto inputArea = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (8, 6);
    inputTitle.setBounds (inputArea.removeFromTop (20));
    inputPanel.setBounds (inputArea);
    bottom.removeFromLeft (gap);
    tabs.setBounds (bottom);

    // Brain columns either side, the morph in the middle.
    brain1Column.setBounds (area.removeFromLeft (columnWidth));
    area.removeFromLeft (gap);
    brain2Column.setBounds (area.removeFromRight (columnWidth));
    area.removeFromRight (gap);

    // The triangles are controls, the plots underneath are what the morph is
    // doing: cap the triangles so the plots get real height.
    constexpr int maxTriangleHeight = 250;
    const int triangleHeight = juce::jmin (area.getWidth() / 2 - gap, maxTriangleHeight, area.getHeight() - 260);
    auto triangles = area.removeFromTop (triangleHeight);
    envelopeTriangle.setBounds (triangles.removeFromLeft (triangles.getWidth() / 2 - gap / 2));
    triangles.removeFromLeft (gap);
    detailTriangle.setBounds (triangles);
    linkToggle.setBounds (area.removeFromTop (28).reduced (4, 2));

    const int remaining = area.getHeight() - gap;
    envelopes.setBounds (area.removeFromTop (remaining / 2));
    area.removeFromTop (gap);
    cepstrum.setBounds (area);
}

void CortexAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    processor.loadBrainFrom (files, 0);
}

void CortexAudioProcessorEditor::timerCallback()
{
    brain1Column.refresh();
    brain2Column.refresh();

    detailTriangle.setFollowing (processor.apvts.getRawParameterValue (cortex::ParamIDs::linkTriangles)->load() > 0.5f);

    const auto levels = processor.getLevels();
    meters.setLevels (levels.inputPeak, levels.outputPeak, levels.gateOpen,
                      processor.apvts.getRawParameterValue (cortex::ParamIDs::gateOn)->load() > 0.5f);

    cortex::dsp::MorphSnapshot snapshot;
    if (processor.readMorphSnapshot (snapshot))
    {
        envelopeTriangle.setLiveWeights (snapshot.envWeights);
        detailTriangle.setLiveWeights (snapshot.detailWeights);
        envelopes.setSnapshot (snapshot);
        cepstrum.setSnapshot (snapshot);
    }
}
