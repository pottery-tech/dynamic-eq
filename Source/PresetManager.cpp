#include "PresetManager.h"
#include "PluginProcessor.h"
using namespace juce;

PresetManager::PresetManager(DynamicEQProcessor& p) : proc(p) {}

File PresetManager::getPresetsFolder() const
{
    File folder = File::getSpecialLocation(File::userDocumentsDirectory)
                      .getChildFile("DynamicEQ")
                      .getChildFile("Presets");
    if (!folder.exists())
        folder.createDirectory();
    return folder;
}

bool PresetManager::savePreset(const String& name)
{
    if (name.isEmpty()) return false;

    MemoryBlock mb;
    proc.getStateInformation(mb);

    auto xml = proc.getXmlFromBinary(mb.getData(), (int)mb.getSize());
    if (!xml) return false;
    xml->setAttribute("presetName", name);

    File file = getPresetsFolder().getChildFile(name + ".deqpreset");
    if (xml->writeTo(file))
    {
        currentPresetName = name;
        return true;
    }
    return false;
}

bool PresetManager::loadPreset(const File& file)
{
    if (!file.existsAsFile()) return false;

    auto xml = XmlDocument::parse(file);
    if (!xml) return false;

    MemoryBlock mb;
    proc.copyXmlToBinary(*xml, mb);
    proc.setStateInformation(mb.getData(), (int)mb.getSize());

    currentPresetName = xml->getStringAttribute("presetName", file.getFileNameWithoutExtension());
    return true;
}

Array<File> PresetManager::getPresetFiles() const
{
    Array<File> files;
    getPresetsFolder().findChildFiles(files, File::findFiles, false, "*.deqpreset");
    files.sort();
    return files;
}

void PresetManager::showSaveDialog(Component* parent)
{
    auto dialog = std::make_shared<AlertWindow>("Save Preset", "Enter a name for this preset:", MessageBoxIconType::NoIcon);
    dialog->addTextEditor("name", currentPresetName.isNotEmpty() ? currentPresetName : "My Preset");
    dialog->addButton("Save",   1);
    dialog->addButton("Cancel", 0);

    dialog->enterModalState(true, ModalCallbackFunction::create([this, dialog](int result)
    {
        if (result == 1)
        {
            String name = dialog->getTextEditorContents("name").trim();
            name = name.replaceCharacters("/\\:*?\"<>|", "_________");
            if (name.isNotEmpty())
                savePreset(name);
        }
    }), false);

    (void)parent;
}

void PresetManager::showLoadMenu(Component* targetComponent)
{
    auto files = getPresetFiles();
    if (files.isEmpty())
    {
        AlertWindow::showMessageBoxAsync(MessageBoxIconType::InfoIcon,
            "No Presets", "No presets found in:\n" + getPresetsFolder().getFullPathName());
        return;
    }

    PopupMenu menu;
    for (int i = 0; i < files.size(); ++i)
        menu.addItem(i + 1, files[i].getFileNameWithoutExtension(),
                     true, files[i].getFileNameWithoutExtension() == currentPresetName);

    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(targetComponent),
        [this, files](int result)
        {
            if (result >= 1 && result <= files.size())
                loadPreset(files[result - 1]);
        });
}
