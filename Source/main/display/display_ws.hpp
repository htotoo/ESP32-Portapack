#ifndef DISPLAY_WS_HPP
#define DISPLAY_WS_HPP

#include "displayskeleton.hpp"
#include "ppshellcomm.h"

bool ws_sendall(uint8_t* data, size_t len, bool asyncmsg);

class Display_Ws : public DisplayGeneric {
   public:
    bool init(uint8_t addr, int sda, int scl) override;
    void clear() override;
    void showTitle(const std::string& title) override;
    void showMainText(const std::string& text) override;
    void showMainTextMultiline(const std::string& text) override;
    void draw() override;

   private:
    std::string title{};
    std::string mainText{};
};

#endif  // DISPLAY_WS_HPP