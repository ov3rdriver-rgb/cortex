#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("Cortex");

    int passes = 0, failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        passes += r->passes;
        failures += r->failures;
        std::printf ("%s / %s: %d passed, %d failed\n", r->unitTestName.toRawUTF8(),
                     r->subcategoryName.toRawUTF8(), r->passes, r->failures);
        for (auto& msg : r->messages)
            std::printf ("  %s\n", msg.toRawUTF8());
    }

    std::printf ("Total: %d passed, %d failed\n", passes, failures);
    return (failures > 0 || passes == 0) ? 1 : 0;
}
