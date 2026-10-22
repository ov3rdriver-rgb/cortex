#pragma once

#include "Theme.h"
#include "brain/Brain.h"

namespace cortex::ui
{
// Every block in the brain as a dot, placed by the first two principal
// components of its MFCCs (similar-sounding blocks land near each other) and
// coloured by source sound. The block playing now is ringed, with a short trail.
class BrainMap final : public juce::Component
{
public:
    void setBrain (std::shared_ptr<const cortex::brain::Brain>);
    void setPlayingBlock (int block);

    void paint (juce::Graphics&) override;
    void resized() override { renderDots(); }

    // Exposed for tests: 2D positions in [0, 1].
    const std::vector<juce::Point<float>>& getPositions() const noexcept { return positions; }
    static juce::Colour colourForSound (int index);

private:
    void computeLayout();
    void renderDots();
    juce::Rectangle<float> mapArea() const;
    juce::Point<float> toScreen (juce::Point<float> p) const;

    std::shared_ptr<const cortex::brain::Brain> brain;
    std::vector<juce::Point<float>> positions;
    std::vector<int> soundOfBlock;
    juce::Image dots;

    std::array<int, 12> trail {};
    int trailLength = 0;
};

// The brain's source sounds with their block counts.
class SoundList final : public juce::Component, private juce::ListBoxModel
{
public:
    SoundList();
    void setBrain (std::shared_ptr<const cortex::brain::Brain>);
    void resized() override { list.setBounds (getLocalBounds()); }

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;

    juce::ListBox list;
    std::shared_ptr<const cortex::brain::Brain> brain;
};
} // namespace cortex::ui
