#include "MainView.h"
#include "Parameters.h"
#include "Tooltips.h"
#include "Version.h"

namespace cortex::ui
{
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

MainView::MainView (CortexAudioProcessor& p)
    : processor (p),
      capture (p),
      brain1Column (p, 0), brain2Column (p, 1),
      envelopeTriangle (p.apvts, cortex::ParamIDs::envX, cortex::ParamIDs::envY, "Envelope (timbre)"),
      detailTriangle (p.apvts, cortex::ParamIDs::detailX, cortex::ParamIDs::detailY, "Detail (pitch, texture)"),
      cepstrum (p.apvts),
      inputPanel (p.apvts, inputIds (p.isInstrument()), "Input ", 74, colours::input),
      help (p.isInstrument())
{
    using namespace cortex::ParamIDs;

    title.setText (p.isInstrument() ? "CORTEX INSTRUMENT" : "CORTEX", juce::dontSendNotification);
    title.setFont (fonts::title (22.0f).withExtraKerningFactor (0.12f));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    helpButton.onClick = [this] { help.show(); };
    addAndMakeVisible (helpButton);

    versionLabel.setText ("v" CORTEX_VERSION_STRING " (" CORTEX_GIT_HASH ")", juce::dontSendNotification);
    versionLabel.setFont (fonts::mono (10.5f));
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (versionLabel);

    addAndMakeVisible (meters);
    addAndMakeVisible (capture);

    addAndMakeVisible (brain1Column);
    addAndMakeVisible (brain2Column);
    addAndMakeVisible (envelopeTriangle);
    addAndMakeVisible (detailTriangle);
    linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, linkTriangles, linkToggle);
    addAndMakeVisible (linkToggle);
    addAndMakeVisible (envelopes);
    addAndMakeVisible (cepstrum);

    inputTitle.setText (fonts::titleCase (p.isInstrument() ? "Input: oscillator" : "Input: track or sidechain"), juce::dontSendNotification);
    inputTitle.setFont (fonts::title (13.0f));
    inputTitle.setColour (juce::Label::textColourId, colours::forSource (cortex::dsp::input));
    addAndMakeVisible (inputTitle);
    addAndMakeVisible (inputPanel);

    // Each brain searches its samples on its own terms: a Search tab per
    // brain, in that brain's colour. (Block size and overlap are in its column.)
    const auto tabColour = colours::panel;
    for (int b = 0; b < CortexAudioProcessor::numBrains; ++b)
    {
        const auto& ids = brainIds[b];
        const auto source = cortex::dsp::brain1 + b;
        searchPanels[(size_t) b] = new ParamPanel (p.apvts, { ids.chaos.id, ids.memory.id, ids.cepstralGlide.id, ids.matchTimbre.id, ids.matchPitch.id,
                                                               ids.envTransplant.id, ids.searchMode.id, ids.novelty.id, ids.boredom.id, ids.stickyness.id },
                                                    juce::String (colours::sourceName (source)) + " ", 84, colours::forSource (source), &p, b);
        tabs.addTab (juce::String (colours::sourceName (source)) + " search", tabColour, searchPanels[(size_t) b], true);
    }

    // Link all: every per-brain control (slicing and search) follows across the brains.
    linkAllButton.setClickingTogglesState (false);
    linkAllButton.setColour (juce::TextButton::buttonOnColourId, colours::neutralAccent.withAlpha (0.28f));
    linkAllButton.setColour (juce::TextButton::textColourOnId, colours::neutralAccent);
    linkAllButton.onClick = [this]
    {
        processor.setAllLinked (! processor.allLinked());
        refresh();
    };
    addAndMakeVisible (linkAllButton);
    tabs.addTab ("Mod", tabColour, new ParamPanel (p.apvts, { lfoRate, lfoDepth, lfoShape, lfoTarget,
                                                              envDepth, envAttack, envRelease }), true);
    tabs.addTab ("Output", tabColour, new ParamPanel (p.apvts, { phaseMode, fftSize, lowLatency, dryWet, outGain,
                                                                 limiter, autoMute, autoMuteMs, captureSeconds, theme }), true);
    tabs.setTabBarDepth (26);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    credits.setText ("AGPL-3.0. Brain engine from Samplebrain (Then Try This / Aphex Twin). Cepstral morph concept: Alexander Panos.",
                     juce::dontSendNotification);
    credits.setFont (fonts::ui (10.5f));
    credits.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (credits);

    addChildComponent (help);   // last, so it sits over everything

    helpButton.setTooltip (tips::helpButton);
    versionLabel.setTooltip (tips::version);
    meters.setTooltip (tips::meters);
    envelopeTriangle.setTooltip (tips::envelopeTriangle);
    detailTriangle.setTooltip (tips::detailTriangle);
    linkToggle.setTooltip (tips::forParam (linkTriangles));
    envelopes.setTooltip (tips::envelopeView);
    cepstrum.setTooltip (tips::cepstrumView);
    auto& bar = tabs.getTabbedButtonBar();
    bar.getTabButton (0)->setTooltip (tips::searchTab (0));
    bar.getTabButton (1)->setTooltip (tips::searchTab (1));
    bar.getTabButton (2)->setTooltip (tips::modTab);
    bar.getTabButton (3)->setTooltip (tips::outputTab);
    linkAllButton.setTooltip (tips::linkAll);

    refresh();
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // The Input region: a hairline edge with the Input's colour as a rule along the top.
    auto input = inputTitle.getBounds().getUnion (inputPanel.getBounds()).expanded (8, 6).toFloat();
    g.setColour (colours::outline);
    g.drawRect (input, 1.0f);
    g.setColour (colours::forSource (cortex::dsp::input));
    g.fillRect (input.removeFromTop (3.0f));

    // A hairline under the header and above the footer.
    g.setColour (colours::rule);
    g.fillRect (gap, headerHeight - 1, getWidth() - 2 * gap, 1);
    g.fillRect (gap, credits.getY() - 3, getWidth() - 2 * gap, 1);
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (gap);

    help.setBounds (getLocalBounds());

    auto header = area.removeFromTop (headerHeight - gap);
    {
        // The title takes what its text needs; the "?" sits right after it.
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (title.getFont(), title.getText(), 0.0f, 0.0f);
        const int titleWidth = (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 12;
        title.setBounds (header.removeFromLeft (juce::jmin (titleWidth, 300)));
        helpButton.setBounds (header.removeFromLeft (26).withSizeKeepingCentre (22, 22));
        header.removeFromLeft (gap);
    }
    versionLabel.setBounds (header.removeFromRight (150));
    meters.setBounds (header.removeFromRight (juce::jmin (260, header.getWidth())));
    header.removeFromRight (gap);
    capture.setBounds (header);   // the button sits at the right, the file chip grows leftwards
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
    linkAllButton.setBounds (bottom.removeFromTop (tabs.getTabBarDepth()).removeFromRight (86).reduced (2, 3));   // at the right end of the tab bar

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

void MainView::refresh()
{
    brain1Column.refresh();
    brain2Column.refresh();
    capture.refresh();
    for (auto* panel : searchPanels)
        panel->refresh();
    linkAllButton.setToggleState (processor.allLinked(), juce::dontSendNotification);

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
} // namespace cortex::ui
