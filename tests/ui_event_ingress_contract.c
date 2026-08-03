#include "ui_evt.h"

void UI_eventIngress_contract(void) {
    UI_postText("text");
    char const portNames[] = "COM3\0COM5\0";
    UI_postPortList(portNames, sizeof(portNames));
}
