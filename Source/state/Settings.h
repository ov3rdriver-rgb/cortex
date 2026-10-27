#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace cortex::settings
{
// Per-user settings that outlive a session (next to the brain cache):
// %APPDATA%/Cortex/Cortex.settings or ~/Library/Application Support/Cortex/.
inline juce::PropertiesFile::Options options()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Cortex";
    o.filenameSuffix = "settings";
    o.folderName = "Cortex";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

inline constexpr const char* themeKey = "theme";   // 0 dark, 1 light

inline int loadTheme (int fallback = 0)
{
    juce::PropertiesFile file (options());
    return file.getIntValue (themeKey, fallback);
}

inline void saveTheme (int index)
{
    juce::PropertiesFile file (options());
    file.setValue (themeKey, index);
    file.saveIfNeeded();
}
} // namespace cortex::settings
