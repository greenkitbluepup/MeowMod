#pragma once

#include "AbstractContainerScreen.h"

class Player;
class Button;

class InventoryScreen : public AbstractContainerScreen
{
public:
    InventoryScreen(shared_ptr<Player> player);

    virtual void init();
    virtual void render(int xm, int ym, float a);

protected:
    virtual void keyPressed(wchar_t eventCharacter, int eventKey);
    virtual void renderLabels();
    virtual void renderBg(float a);
    virtual void buttonClicked(Button* button);

private:
    float xMouse;
    float yMouse;
};