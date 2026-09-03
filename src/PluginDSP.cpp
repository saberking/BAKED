/*
 * ImGui plugin example
 * Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
 * Copyright (C) 2021-2025 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "PluginDSP.hpp"

START_NAMESPACE_DISTRHO



// --------------------------------------------------------------------------------------------------------------------




// --------------------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new ImGuiPluginDSP();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
