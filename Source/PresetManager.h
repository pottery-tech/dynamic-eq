#pragma once
#include <JuceHeader.h>

class DynamicEQProcessor;

//==============================================================================
/** Handles save/load of named presets to/from the user's Documents folder.
 *  Presets are stored as XML in:
 *    Documents/DynamicEQ/Presets/<name>.deqpreset
 */
class PresetManager
{
public:
    explicit PresetManager(DynamicEQProcessor& proc);

    // Returns the presets folder, creating it if needed
    juce::File getPresetsFolder() const;

    // Save current state as a named preset
    bool savePreset(const juce::String& name);

    // Load a preset by file
    bool loadPreset(const juce::File& file);

    // List all available preset files
    juce::Array<juce::File> getPresetFiles() const;

    // Show save dialog, then save
    void showSaveDialog(juce::Component* parent);

    // Show popup menu of presets and load selected
    void showLoadMenu(juce::Component* targetComponent);

    // Current preset name (empty if unsaved state)
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName(const juce::String& n) { currentPresetName = n; }

private:
    DynamicEQProcessor& proc;
    juce::String currentPresetName;
};
