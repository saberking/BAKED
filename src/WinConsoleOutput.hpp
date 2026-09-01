#include <windows.h>
#include <iostream>
#include "src/DistrhoDefines.h"

#ifndef WINCONSOLEOUTPUT_HPP
#define WINCONSOLEOUTPUT_HPP
START_NAMESPACE_DISTRHO

inline void initConsoleOutput(){
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    std::cout << "VST Console Initialized!" << std::endl;
}

END_NAMESPACE_DISTRHO
#endif // WINCONSOLEOUTPUT_HPP
