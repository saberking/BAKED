#ifndef PLUGINUI_HPP
#define PLUGINUI_HPP


#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "DistrhoUI.hpp"
#include "ResizeHandle.hpp"
#include "Parameters.hpp"

#include <windows.h>
#include <shellapi.h>
#include <ole2.h>

START_NAMESPACE_DISTRHO

class ImGuiPluginUI : public UI
{
    float fRelease = 0.0f;
    char sampleFilePath[256];
    ResizeHandle fResizeHandle;

public:
    ImGuiPluginUI();


    void setDroppedFilePath(const char* path);

protected:
    void parameterChanged(uint32_t index, float value) override;

    void onImGuiDisplay() override;


    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginUI)
};


END_NAMESPACE_DISTRHO

#endif // PLUGINUI_HPP
