#include "Theme.h"

namespace cortex::ui
{
JUCE_IMPLEMENT_SINGLETON (Faces)

Faces::Faces()
{
    auto load = [] (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
    inter = load (CortexFonts::InterRegular_ttf, CortexFonts::InterRegular_ttfSize);
    interBold = load (CortexFonts::InterSemiBold_ttf, CortexFonts::InterSemiBold_ttfSize);
    barlow = load (CortexFonts::BarlowRegular_ttf, CortexFonts::BarlowRegular_ttfSize);
    barlowBold = load (CortexFonts::BarlowSemiBold_ttf, CortexFonts::BarlowSemiBold_ttfSize);
}
} // namespace cortex::ui
