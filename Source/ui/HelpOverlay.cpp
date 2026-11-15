#include "HelpOverlay.h"

namespace cortex::ui
{
namespace
{
    constexpr int pageMargin = 28;
    constexpr int maxPanelWidth = 760;

    struct Section
    {
        const char* title;
        juce::Colour colour;
        juce::String body;
    };

    std::vector<Section> sections (bool instrument)
    {
        const juce::String input = instrument ? "the oscillator you play with MIDI" : "the track Cortex sits on, or a sidechain";
        std::vector<Section> s;

        s.push_back ({ "What Cortex does", colours::neutralAccent,
            "Cortex is a sampler that plays itself. You load folders of samples into two brains. Each brain chops its "
            "samples into short slices (about 20 ms at the default block size) and analyses every slice: its tone colour, "
            "its pitch, how noisy it is.\n\n"
            "While you play, each brain listens to a signal (your Input by default: " + input + ") and keeps picking the "
            "slice of its own samples that sounds closest to what it hears right now. Think of two improvisers who can only "
            "reply with bits of their own record collection. Brain 1 and Brain 2 can hold different collections, chop them "
            "differently and search them differently.\n\n"
            "Cortex then blends three sources, your Input, Brain 1 and Brain 2, with a cepstral morph. That blend is the output." });

        s.push_back ({ "The cepstral morph", colours::result,
            "A cepstrum is the spectrum of a spectrum. Take a short window of sound, work out its spectrum, take the "
            "logarithm, and transform that again. What comes out separates the sound into two layers:\n\n"
            "Envelope: the slow part of the spectrum, the overall shape. The body, the colour, the vowel-like quality. "
            "What makes a violin sound like a violin whatever note it plays.\n\n"
            "Detail: the fast part. Which notes and harmonics are present, the grit, the noise, the texture.\n\n"
            "The cutoff line for each source in the cepstrum view is where its envelope ends and its detail begins. Drag "
            "it: higher means more of that source counts as envelope.\n\n"
            "Cortex mixes the two layers separately. The Envelope triangle sets how much of each source's envelope goes "
            "into the result; the Detail triangle does the same for the detail. Then it puts the layers back together, "
            "takes the phase from the source you choose (Output tab), and turns the result back into sound.\n\n"
            "So you can take the notes from one source and the body of another: your melody wearing the colour of a drum "
            "break, or a breakbeat that sings at your synth's pitch." });

        s.push_back ({ "The triangles", colours::input,
            "Each triangle has Brain 1 at the top, Brain 2 at the bottom left and the Input at the bottom right. Drag the "
            "dot; each corner shows its share. Link makes the Detail triangle follow the Envelope triangle. A brain with "
            "nothing loaded hands its share back to the Input.\n\n"
            "The default is the envelope from Brain 1 and the detail from the Input: the brain's colour on your notes." });

        s.push_back ({ "How a brain chooses its slices", colours::brain1,
            "Listen (top of the column): what the brain matches. The Input, the track or sidechain on its own, the other "
            "brain, or Cortex's own output.\n\n"
            "Block Size and Block Overlap: how its samples are chopped. Short slices follow closely and sound granular; "
            "long slices keep more of each sample's character.\n\n"
            "Match Timbre and Match Pitch (its Search tab): what \"closest\" means. Colour, notes, or a mix.\n\n"
            "Chaos makes it wander; Memory makes it remember what it played and follow runs in its samples. Search Mode: "
            "Basic picks the closest slice, Rev the furthest, Synaptic walks to a neighbour of the current slice so the "
            "brain drifts rather than jumps.\n\n"
            "Transplant pushes the Input's envelope onto the brain's slices before the morph. Cepstral Glide slides between "
            "slices' colours instead of cutting.\n\n"
            "The small rings in a control's corner link it between the brains; Link all links everything." });

        s.push_back ({ "A first sound", colours::brain2,
            instrument
                ? juce::String ("1. Drop a folder of samples on the Brain 1 column and wait for the block count.\n"
                                "2. Play MIDI notes. The Input is the oscillator in the Input panel.\n"
                                "3. Drag the dot in the Envelope triangle and listen to the colour change.\n"
                                "4. Drop a different folder on Brain 2 and move both dots.")
                : juce::String ("1. Put Cortex on a track that has a sound.\n"
                                "2. Drop a folder of samples on the Brain 1 column and wait for the block count.\n"
                                "3. Play the track. Drag the dot in the Envelope triangle and listen to the colour change.\n"
                                "4. Drop a different folder on Brain 2 and move both dots.") });

        s.push_back ({ "Everything else", colours::dim,
            "Hover any control for a few sentences on what it does. Capture, in the header, saves the last minute of output "
            "as a WAV you can drag into your DAW. Low Latency (Output tab) is for playing live." });
        return s;
    }
}

HelpOverlay::Page::Page (bool instrument)
{
    for (const auto& s : sections (instrument))
    {
        text.append (fonts::titleCase (s.title) + "\n", fonts::title (12.0f), s.colour);
        text.append ("\n", fonts::ui (6.0f), colours::text);
        text.append (s.body + "\n", fonts::ui (13.5f), colours::text);
        text.append ("\n", fonts::ui (14.0f), colours::text);
    }
    text.setLineSpacing (3.0f);
    setInterceptsMouseClicks (false, false);
}

void HelpOverlay::Page::setPageWidth (int width)
{
    layout.createLayout (text, (float) (width - 2 * pageMargin));
    setSize (width, (int) std::ceil (layout.getHeight()) + 2 * pageMargin);
}

void HelpOverlay::Page::paint (juce::Graphics& g)
{
    layout.draw (g, getLocalBounds().reduced (pageMargin).toFloat());
}

juce::String HelpOverlay::plainText (bool instrument)
{
    juce::String out;
    for (const auto& s : sections (instrument))
        out << s.title << "\n" << s.body << "\n\n";
    return out;
}

HelpOverlay::HelpOverlay (bool instrument) : page (instrument)
{
    heading.setText (fonts::titleCase (instrument ? "Cortex Instrument: how it works" : "Cortex: how it works"), juce::dontSendNotification);
    heading.setFont (fonts::title (14.0f));
    heading.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (heading);

    closeButton.onClick = [this] { hide(); };
    addAndMakeVisible (closeButton);

    viewport.setViewedComponent (&page, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    setWantsKeyboardFocus (true);
    setVisible (false);
}

void HelpOverlay::show()
{
    setVisible (true);
    toFront (true);
    viewport.setViewPosition (0, 0);
    grabKeyboardFocus();
}

void HelpOverlay::hide()
{
    setVisible (false);
}

void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (colours::background.withAlpha (0.82f));
    g.setColour (colours::panel);
    g.fillRect (panel);
    g.setColour (colours::outline);
    g.drawRect (panel, 1);
    g.setColour (colours::neutralAccent);
    g.fillRect (panel.withHeight (3));
}

void HelpOverlay::resized()
{
    panel = getLocalBounds().reduced (juce::jmax (24, (getWidth() - maxPanelWidth) / 2), juce::jmax (24, getHeight() / 12));
    auto inner = panel.reduced (1);
    auto top = inner.removeFromTop (40);
    closeButton.setBounds (top.removeFromRight (80).reduced (12, 8));
    heading.setBounds (top.withTrimmedLeft (pageMargin));
    viewport.setBounds (inner);
    page.setPageWidth (inner.getWidth() - viewport.getScrollBarThickness());
}

void HelpOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! panel.contains (e.getPosition()))
        hide();
}

bool HelpOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }
    return false;
}
} // namespace cortex::ui
