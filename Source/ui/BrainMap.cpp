#include "BrainMap.h"

namespace cortex::ui
{
namespace
{
    constexpr int pcaDims = cortex::brain::numMfcc - 1;   // MFCC 1..12; c0 is just level

    using Vec = std::array<double, pcaDims>;
    using Mat = std::array<Vec, pcaDims>;

    // Dominant eigenvector of a symmetric matrix by power iteration.
    Vec dominantEigenvector (const Mat& m)
    {
        Vec v {};
        for (int i = 0; i < pcaDims; ++i)
            v[(size_t) i] = 1.0 + 0.01 * i;   // not orthogonal to anything in practice

        for (int iter = 0; iter < 200; ++iter)
        {
            Vec next {};
            for (int r = 0; r < pcaDims; ++r)
                for (int c = 0; c < pcaDims; ++c)
                    next[(size_t) r] += m[(size_t) r][(size_t) c] * v[(size_t) c];

            double norm = 0.0;
            for (auto x : next)
                norm += x * x;
            norm = std::sqrt (norm);
            if (norm < 1.0e-12)
                break;
            for (auto& x : next)
                x /= norm;
            v = next;
        }
        return v;
    }
}

juce::Colour BrainMap::colourForSound (int index)
{
    // Golden-ratio hue steps keep neighbouring sounds distinct.
    const float hue = std::fmod (0.08f + 0.618034f * (float) index, 1.0f);
    return juce::Colour::fromHSV (hue, 0.55f, 0.95f, 1.0f);
}

void BrainMap::setBrain (std::shared_ptr<const cortex::brain::Brain> newBrain)
{
    if (newBrain == brain)
        return;
    brain = std::move (newBrain);
    trailLength = 0;
    computeLayout();
    renderDots();
    repaint();
}

void BrainMap::setPlayingBlock (int block)
{
    if (block < 0 || (trailLength > 0 && trail[0] == block))
        return;
    for (int i = (int) trail.size() - 1; i > 0; --i)
        trail[(size_t) i] = trail[(size_t) (i - 1)];
    trail[0] = block;
    trailLength = juce::jmin ((int) trail.size(), trailLength + 1);
    repaint();
}

void BrainMap::computeLayout()
{
    positions.clear();
    soundOfBlock.clear();
    if (brain == nullptr || brain->blocks.empty())
        return;

    const auto n = brain->blocks.size();

    Vec mean {};
    for (const auto& b : brain->blocks)
        for (int d = 0; d < pcaDims; ++d)
            mean[(size_t) d] += b.features.mfcc[(size_t) (d + 1)];
    for (auto& m : mean)
        m /= (double) n;

    Mat cov {};
    for (const auto& b : brain->blocks)
    {
        Vec v {};
        for (int d = 0; d < pcaDims; ++d)
            v[(size_t) d] = b.features.mfcc[(size_t) (d + 1)] - mean[(size_t) d];
        for (int r = 0; r < pcaDims; ++r)
            for (int c = 0; c < pcaDims; ++c)
                cov[(size_t) r][(size_t) c] += v[(size_t) r] * v[(size_t) c];
    }

    const auto pc1 = dominantEigenvector (cov);
    // Deflate to get the second component.
    double lambda = 0.0;
    for (int r = 0; r < pcaDims; ++r)
        for (int c = 0; c < pcaDims; ++c)
            lambda += pc1[(size_t) r] * cov[(size_t) r][(size_t) c] * pc1[(size_t) c];
    for (int r = 0; r < pcaDims; ++r)
        for (int c = 0; c < pcaDims; ++c)
            cov[(size_t) r][(size_t) c] -= lambda * pc1[(size_t) r] * pc1[(size_t) c];
    const auto pc2 = dominantEigenvector (cov);

    std::vector<float> xs (n), ys (n);
    for (size_t i = 0; i < n; ++i)
    {
        double px = 0.0, py = 0.0;
        for (int d = 0; d < pcaDims; ++d)
        {
            const double v = brain->blocks[i].features.mfcc[(size_t) (d + 1)] - mean[(size_t) d];
            px += v * pc1[(size_t) d];
            py += v * pc2[(size_t) d];
        }
        xs[i] = (float) px;
        ys[i] = (float) py;
    }

    // Robust range (1st..99th percentile) so a few outliers don't squash the map.
    auto range = [] (std::vector<float> v)
    {
        const auto lo = v.size() / 100, hi = v.size() - 1 - v.size() / 100;
        std::nth_element (v.begin(), v.begin() + (std::ptrdiff_t) lo, v.end());
        const float a = v[lo];
        std::nth_element (v.begin(), v.begin() + (std::ptrdiff_t) hi, v.end());
        const float b = v[hi];
        return std::pair { a, juce::jmax (b, a + 1.0e-6f) };
    };
    const auto [x0, x1] = range (xs);
    const auto [y0, y1] = range (ys);

    positions.resize (n);
    for (size_t i = 0; i < n; ++i)
        positions[i] = { juce::jlimit (0.0f, 1.0f, (xs[i] - x0) / (x1 - x0)),
                         juce::jlimit (0.0f, 1.0f, (ys[i] - y0) / (y1 - y0)) };

    soundOfBlock.assign (n, 0);
    for (size_t s = 0; s < brain->sounds.size(); ++s)
        for (auto i = brain->sounds[s].firstBlock; i < brain->sounds[s].firstBlock + brain->sounds[s].numBlocks && i < n; ++i)
            soundOfBlock[i] = (int) s;
}

juce::Rectangle<float> BrainMap::mapArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f).withTrimmedTop (10.0f);
}

juce::Point<float> BrainMap::toScreen (juce::Point<float> p) const
{
    const auto a = mapArea();
    return { a.getX() + p.x * a.getWidth(), a.getBottom() - p.y * a.getHeight() };
}

void BrainMap::renderDots()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    dots = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (dots);

    // Big brains get smaller, fainter dots so the density still reads.
    const float size = positions.size() > 50000 ? 1.5f : positions.size() > 5000 ? 2.0f : 3.0f;
    const float alpha = positions.size() > 50000 ? 0.25f : positions.size() > 5000 ? 0.45f : 0.8f;
    for (size_t i = 0; i < positions.size(); ++i)
    {
        g.setColour (colourForSound (soundOfBlock[i]).withAlpha (alpha));
        const auto p = toScreen (positions[i]);
        g.fillRect (p.x - size * 0.5f, p.y - size * 0.5f, size, size);
    }
}

void BrainMap::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (colours::dim);
    g.drawText ("brain map (blocks by timbre, PCA of MFCCs)", getLocalBounds().reduced (12, 4).removeFromTop (14),
                juce::Justification::centredLeft);

    if (positions.empty())
    {
        g.drawText ("no brain loaded", getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.drawImageAt (dots, 0, 0);

    for (int i = trailLength - 1; i >= 0; --i)
    {
        const int block = trail[(size_t) i];
        if (block < 0 || block >= (int) positions.size())
            continue;
        const auto p = toScreen (positions[(size_t) block]);
        const float fade = 1.0f - (float) i / (float) trail.size();
        if (i == 0)
        {
            g.setColour (colours::text);
            g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (p), 2.0f);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p));
        }
        else
        {
            g.setColour (colours::text.withAlpha (0.5f * fade));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p));
        }
    }
}

SoundList::SoundList()
{
    list.setModel (this);
    list.setRowHeight (18);
    list.setColour (juce::ListBox::backgroundColourId, colours::panel);
    addAndMakeVisible (list);
}

void SoundList::setBrain (std::shared_ptr<const cortex::brain::Brain> newBrain)
{
    if (newBrain == brain)
        return;
    brain = std::move (newBrain);
    list.updateContent();
    list.repaint();
}

int SoundList::getNumRows()
{
    return brain != nullptr ? (int) brain->sounds.size() : 0;
}

void SoundList::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    if (brain == nullptr || row < 0 || row >= (int) brain->sounds.size())
        return;
    const auto& s = brain->sounds[(size_t) row];

    g.setColour (BrainMap::colourForSound (row));
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ 10.0f, height * 0.5f }));

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (colours::text);
    g.drawText (juce::File (s.path).getFileName(), 22, 0, width - 92, height, juce::Justification::centredLeft, true);
    g.setColour (colours::dim);
    g.drawText (juce::String ((int) s.numBlocks) + " blocks", width - 70, 0, 64, height, juce::Justification::centredRight);
}
} // namespace cortex::ui
