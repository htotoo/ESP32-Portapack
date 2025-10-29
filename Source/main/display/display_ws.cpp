#include "display_ws.hpp"
#include <algorithm>

bool Display_Ws::init(uint8_t addr, int sda, int scl) {
    return true;
};

void Display_Ws::clear() {
    title.clear();
    mainText.clear();
}

void Display_Ws::showTitle(const std::string& title) {
    this->title = title;
    std::replace(this->title.begin(), this->title.end(), '\n', ' ');
}

void Display_Ws::showMainText(const std::string& text) {
    showMainTextMultiline(text);
}

void Display_Ws::showMainTextMultiline(const std::string& text) {
    mainText = text;
    size_t pos = 0;
    while ((pos = mainText.find("\n", pos)) != std::string::npos) {
        mainText.replace(pos, 1, "\\n");
        pos += 2;  // Move past the inserted "\\n"
    }
}

void Display_Ws::draw() {
    if (PPShellComm::getInCommand()) return;  // skip this frame
    char buff[400];
    snprintf(buff, 400, "#$##$$#GOTDISPLAYTITLE%s\r\n", (char*)title.c_str());
    ws_sendall((uint8_t*)buff, strlen(buff), true);
    snprintf(buff, 400, "#$##$$#GOTDISPLAYMAIN%s\r\n", (char*)mainText.c_str());
    ws_sendall((uint8_t*)buff, strlen(buff), true);
}