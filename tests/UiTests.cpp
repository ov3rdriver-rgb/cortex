#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
juce::File makeBrainFolder (int seed)
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-ui-" + juce::String (seed));
    dir.deleteRecursively();
    dir.createDirectory();

    // Three distinct sounds so the map has something to separate.
    for (int s = 0; s < 3; ++s)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (dir.getChildFile ("sound" + juce::String (s) + ".wav")),
                                                                         48000.0, 1, 24, {}, 0));
        juce::AudioBuffer<float> a (1, 72000);
        juce::Random rng (seed + s);
        double phase = 0.0;
        float lp = 0.0f;
        for (int i = 0; i < a.getNumSamples(); ++i)
        {
            phase += (110.0 * (s + 1) + 40.0 * std::sin (i * 0.0002)) / 48000.0;
            lp += (0.05f + 0.3f * (float) s) * ((rng.nextFloat() - 0.5f) - lp);
            const float tone = (float) (2.0 * (phase - std::floor (phase)) - 1.0);
            a.setSample (0, i, s == 0 ? 0.3f * tone : s == 1 ? 0.6f * lp : 0.2f * tone + 0.3f * lp);
        }
        w->writeFromAudioSampleBuffer (a, 0, a.getNumSamples());
    }
    return dir;
}

void runAudio (CortexAudioProcessor& proc, int blocks)
{
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double phase = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (proc.isInstrument() && b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.9f), 0);
        for (int i = 0; i < 512; ++i)
        {
            phase += 165.0 / 48000.0;
            const float s = proc.isInstrument() ? 0.0f : (float) (0.4 * (2.0 * (phase - std::floor (phase)) - 1.0));
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
        }
        proc.processBlock (buf, midi);
    }
}
} // namespace

class EditorTests final : public juce::UnitTest
{
public:
    EditorTests() : juce::UnitTest ("Editor", "Cortex") {}

    void runTest() override
    {
        const auto dir = makeBrainFolder (77), dir2 = makeBrainFolder (91);
        const auto outDir = juce::File (juce::SystemStats::getEnvironmentVariable ("CORTEX_DIAG_OUT",
                                            juce::File::getSpecialLocation (juce::File::tempDirectory).getFullPathName()));

        for (bool instrument : { false, true })
        {
            beginTest (juce::String ("Editor builds, lays out at several sizes and shows live data: ") + (instrument ? "instrument" : "effect"));

            CortexAudioProcessor proc (instrument);
            auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            set (cortex::ParamIDs::envX, 0.45f);    // envelope: mostly Brain 1, some Brain 2 and Input
            set (cortex::ParamIDs::envY, 0.55f);
            set (cortex::ParamIDs::cutoffB1, 40.0f);
            set (cortex::ParamIDs::cutoffB2, 60.0f);
            proc.setNonRealtime (true);
            proc.prepareToPlay (48000.0, 512);
            proc.loadBrainFrom ({ dir.getFullPathName() }, 0);
            proc.loadBrainFrom ({ dir2.getFullPathName() }, 1);
            for (int i = 0; i < 600 && (proc.getBrain (0) == nullptr || proc.getBrain (1) == nullptr); ++i)
                juce::Thread::sleep (50);
            expect (proc.getBrain (0) != nullptr && proc.getBrain (1) != nullptr);
            runAudio (proc, 200);

            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            auto* cortexEditor = dynamic_cast<CortexAudioProcessorEditor*> (editor.get());
            expect (cortexEditor != nullptr);

            for (auto [w, h] : { std::pair { 1120, 760 }, std::pair { 1700, 1100 }, std::pair { 1280, 820 } })
                editor->setSize (w, h);

            cortexEditor->refresh();
            cortex::dsp::MorphSnapshot snap;
            expect (proc.readMorphSnapshot (snap) && snap.valid);

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
            const auto file = outDir.getChildFile (instrument ? "ui_instrument.png" : "ui_effect.png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            juce::PNGImageFormat().writeImageToStream (image, out);
            logMessage ("  wrote " + file.getFullPathName());

            editor.reset();
        }

        beginTest ("Brain map: one position per block, all within the map");
        {
            CortexAudioProcessor proc;
            proc.prepareToPlay (48000.0, 512);
            proc.loadBrainFrom ({ dir.getFullPathName() });
            for (int i = 0; i < 600 && proc.getBrain() == nullptr; ++i)
                juce::Thread::sleep (50);

            cortex::ui::BrainMap map;
            map.setSize (400, 300);
            map.setBrain (proc.getBrain());
            expectEquals ((int) map.getPositions().size(), (int) proc.getBrain()->numBlocks());
            bool inside = true;
            for (auto p : map.getPositions())
                inside &= p.x >= 0.0f && p.x <= 1.0f && p.y >= 0.0f && p.y <= 1.0f;
            expect (inside);
        }

        dir.deleteRecursively();
        dir2.deleteRecursively();
    }
};

static EditorTests editorTests;
