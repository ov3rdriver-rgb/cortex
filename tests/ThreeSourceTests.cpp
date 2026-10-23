// The three-source morph: Input, Brain 1 and Brain 2 on two triangles.

#include "PluginProcessor.h"
#include "Parameters.h"
#include "TestHelpers.h"

#include <cmath>
#include <vector>

using namespace cortex::dsp;

namespace
{
std::vector<float> noise (size_t length, int seed, float level)
{
    juce::Random rng (seed);
    std::vector<float> v (length);
    for (auto& s : v)
        s = level * (rng.nextFloat() * 2.0f - 1.0f);
    return v;
}

std::vector<float> runMorph3 (const PerSource<std::vector<float>>& in, const MorphParams& p, int order)
{
    STFT stft;
    stft.prepare();
    stft.setOrder (order);
    CepstralMorph morph;
    morph.prepare (48000.0);
    morph.setOrder (order);
    morph.setTargets (p);
    morph.snapToTargets();

    std::vector<float> out (in[0].size());
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = stft.processSample ({ in[0][i], in[1][i], in[2][i] },
                                     [&] (const STFT::Frames& f, float* fo) { morph.processFrame (f, fo); });
    return out;
}

double nullDb (const std::vector<float>& out, const std::vector<float>& ref, int latency)
{
    double err = 0.0, sig = 0.0;
    for (size_t i = (size_t) latency * 2; i < out.size(); ++i)
    {
        const double r = ref[i - (size_t) latency];
        err += (out[i] - r) * (out[i] - r);
        sig += r * r;
    }
    return 10.0 * std::log10 (err / sig + 1.0e-30);
}
} // namespace

class ThreeSourceTests final : public juce::UnitTest
{
public:
    ThreeSourceTests() : juce::UnitTest ("Three-source morph", "Cortex") {}

    void runTest() override
    {
        beginTest ("Triangle weights: corners, centre, and points outside");
        {
            auto near = [this] (const PerSource<float>& w, float in, float b1, float b2)
            {
                expectWithinAbsoluteError (w[input], in, 1.0e-5f);
                expectWithinAbsoluteError (w[brain1], b1, 1.0e-5f);
                expectWithinAbsoluteError (w[brain2], b2, 1.0e-5f);
            };
            near (triangleWeights (triangleCorner[inputCorner].x, triangleCorner[inputCorner].y), 1, 0, 0);
            near (triangleWeights (triangleCorner[brain1Corner].x, triangleCorner[brain1Corner].y), 0, 1, 0);
            near (triangleWeights (triangleCorner[brain2Corner].x, triangleCorner[brain2Corner].y), 0, 0, 1);
            near (triangleWeights (0.5f, 1.0f / 3.0f), 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f);

            // Every point of the square gives weights in 0..1 that sum to 1.
            bool valid = true;
            for (float x = 0.0f; x <= 1.0f; x += 0.05f)
                for (float y = 0.0f; y <= 1.0f; y += 0.05f)
                {
                    const auto w = triangleWeights (x, y);
                    valid &= std::abs (w[0] + w[1] + w[2] - 1.0f) < 1.0e-5f;
                    for (auto v : w)
                        valid &= v >= 0.0f && v <= 1.0f;
                }
            expect (valid);
        }

        const size_t len = 48000;
        const PerSource<std::vector<float>> sources { noise (len, 1, 0.5f), noise (len, 2, 0.3f), noise (len, 3, 0.4f) };

        for (int s = 0; s < numSources; ++s)
        {
            beginTest ("Each corner reproduces its source exactly (source " + juce::String (s) + ")");
            MorphParams p;
            p.envWeights = p.detailWeights = { 0.0f, 0.0f, 0.0f };
            p.envWeights[(size_t) s] = p.detailWeights[(size_t) s] = 1.0f;
            const double db = nullDb (runMorph3 (sources, p, 11), sources[(size_t) s], 2048);
            logMessage ("  null: " + juce::String (db, 1) + " dB");
            expectLessThan (db, -60.0);
        }

        beginTest ("A source with no weight has no effect (Input-Brain 1 edge ignores Brain 2)");
        {
            const auto p = cortex::test::abParams (0.4f, 0.7f);
            auto withSilentBrain2 = sources;
            std::fill (withSilentBrain2[brain2].begin(), withSilentBrain2[brain2].end(), 0.0f);
            expect (runMorph3 (sources, p, 11) == runMorph3 (withSilentBrain2, p, 11));
        }

        beginTest ("Link Triangles: the detail follows the envelope");
        {
            CortexAudioProcessor proc (false);
            auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            cortex::test::setTriangleCorner (proc.apvts, true, Source::brain2);    // envelope: Brain 2
            cortex::test::setTriangleCorner (proc.apvts, false, Source::input);    // detail: Input
            set (cortex::ParamIDs::linkTriangles, 1.0f);
            set (cortex::ParamIDs::limiter, 0.0f);
            proc.prepareToPlay (48000.0, 512);

            // With Brain 2 loaded (so its weight isn't handed back to the Input),
            // the detail weights the morph receives show whether the link took.
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cortex-link-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            dir.createDirectory();
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (dir.getChildFile ("b.wav")), 48000.0, 1, 24, {}, 0));
                const auto audio = noise (96000, 9, 0.3f);
                const float* chans[] { audio.data() };
                w->writeFromFloatArrays (chans, 1, (int) audio.size());
            }
            proc.loadBrainFrom ({ dir.getFullPathName() }, 1);
            for (int i = 0; i < 600 && proc.getBrain (1) == nullptr; ++i)
                juce::Thread::sleep (50);

            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            const auto in = noise (512 * 200, 4, 0.3f);
            for (int b = 0; b < 200; ++b)
            {
                for (int ch = 0; ch < 2; ++ch)
                    buf.copyFrom (ch, 0, in.data() + b * 512, 512);
                proc.processBlock (buf, midi);
            }
            MorphSnapshot snap;
            expect (proc.readMorphSnapshot (snap));
            logMessage ("  detail weights with link on: input " + juce::String (snap.detailWeights[input], 2)
                        + ", brain 2 " + juce::String (snap.detailWeights[brain2], 2));
            expectWithinAbsoluteError (snap.detailWeights[brain2], 1.0f, 0.01f);
            dir.deleteRecursively();
        }
    }
};

static ThreeSourceTests threeSourceTests;
