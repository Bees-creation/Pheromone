#ifdef PHEROMONE_DEBUG
#include <intrin.h>
#define debug_assert() __debugbreak()
#else
#define debug_assert()
#endif

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "serial_assistant.h"

int main() {
    SerialAssistant appWindow(960, 540, "串口助手");

    appWindow.show();
    return Fl::run();
}
