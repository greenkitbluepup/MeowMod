#include "stdafx.h"
#include "InventoryScreen.h"

#include "MultiplayerLocalPlayer.h"
#include "Font.h"
#include "EntityRenderDispatcher.h"
#include "Lighting.h"
#include "Textures.h"
#include "Button.h"
#include "AchievementScreen.h"
#include "StatsScreen.h"
#include "Options.h"
#include "KeyMapping.h"
#include "Tesselator.h"
#ifdef _WINDOWS64
#include "Windows64/KeyboardMouseInput.h"
#endif

#include "..\Minecraft.World\net.minecraft.stats.h"
#include "..\Minecraft.World\net.minecraft.world.entity.player.h"

InventoryScreen::InventoryScreen(shared_ptr<Player> player)
    : AbstractContainerScreen(player->inventoryMenu)
{
    xMouse = 0.0f;
    yMouse = 0.0f;

    // This is now a real blocking C++ screen, not an Iggy overlay shell.
    this->passEvents = false;

    // Modern Java inventory background size.
    imageWidth = 176;
    imageHeight = 166;

    player->awardStat(GenericStats::openInventory(), GenericStats::param_noArgs());
}

void InventoryScreen::init()
{
    AbstractContainerScreen::init();

    // Do not keep old achievement/stats buttons in the modern inventory pass.
    buttons.clear();
}

void InventoryScreen::keyPressed(wchar_t eventCharacter, int eventKey)
{
    bool closeInventory = false;

    if (eventKey == Keyboard::KEY_ESCAPE)
        closeInventory = true;

#ifdef _WINDOWS64
    if (eventKey == KeyboardMouseInput::KEY_INVENTORY)
        closeInventory = true;
#endif

    if (minecraft && minecraft->options && minecraft->options->keyBuild &&
        eventKey == minecraft->options->keyBuild->key)
    {
        closeInventory = true;
    }

    if (closeInventory)
    {
        if (minecraft->player != nullptr)
            minecraft->player->closeContainer();

        minecraft->setScreen(nullptr);
        return;
    }

    AbstractContainerScreen::keyPressed(eventCharacter, eventKey);
}

void InventoryScreen::renderLabels()
{
    // Java-style label position.
    font->draw(L"Crafting", 97, 6, 0x404040);
}

void InventoryScreen::render(int xm, int ym, float a)
{
    AbstractContainerScreen::render(xm, ym, a);
}

void InventoryScreen::renderBg(float a)
{
    // Use the live container mouse (green-dot/source-of-truth), not render(xm, ym).
    xMouse = static_cast<float>(getLiveMouseX());
    yMouse = static_cast<float>(getLiveMouseY());

    // Modern Java inventory background.
    // Asset should exist at:
    // Minecraft.Client/Common/res/1_2_2/gui/container/inventory.png
  glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    RenderManager.StateSetBlendFactor(0xffffffff);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    minecraft->textures->bindTexture(L"gui/container/inventory.png");

    const float u0 = 0.0f;
    const float v0 = 0.0f;
    const float u1 = 176.0f / 256.0f;
    const float v1 = 166.0f / 256.0f;

    Tesselator* t = Tesselator::getInstance();
    t->begin();
    t->vertexUV(0.0f, 166.0f, 0.0f, u0, v1);
    t->vertexUV(176.0f, 166.0f, 0.0f, u1, v1);
    t->vertexUV(176.0f, 0.0f, 0.0f, u1, v0);
    t->vertexUV(0.0f, 0.0f, 0.0f, u0, v0);
    t->end();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    RenderManager.StateSetBlendFactor(0xffffffff);

    // Player preview.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(true);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_RESCALE_NORMAL);
    glEnable(GL_COLOR_MATERIAL);
    glDisable(GL_BLEND);

    glPushMatrix();

    // Modern Java player model position:
    // x = leftPos + 51
    // y = topPos + 75
    glTranslatef(51.0f, 75.0f, 50.0f);

    float ss = 30.0f;
    glScalef(-ss, ss, ss);
    glRotatef(180, 0, 0, 1);

    float oldBodyRot = minecraft->player->yBodyRot;
    float oldBodyRotO = minecraft->player->yBodyRotO;
    float oldHeadRot = minecraft->player->yHeadRot;
    float oldHeadRotO = minecraft->player->yHeadRotO;
    float oldYRot = minecraft->player->yRot;
    float oldYRotO = minecraft->player->yRotO;
    float oldXRot = minecraft->player->xRot;
    float oldXRotO = minecraft->player->xRotO;

    float xd = static_cast<float>(this->leftPos + 51) - xMouse;
    float yd = static_cast<float>(this->topPos + 75 - 50) - yMouse;

    glRotatef(45.0f + 90.0f, 0, 1, 0);
    Lighting::turnOn();
    glRotatef(-45.0f - 90.0f, 0, 1, 0);

    glRotatef(-(float)atan(yd / 40.0f) * 20.0f, 1, 0, 0);

    float yawBody = (float)atan(xd / 40.0f) * 20.0f;
    float yawHead = (float)atan(xd / 40.0f) * 40.0f;
    float pitch = -(float)atan(yd / 40.0f) * 20.0f;

    minecraft->player->yBodyRot = yawBody;
    minecraft->player->yBodyRotO = yawBody;
    minecraft->player->yHeadRot = yawHead;
    minecraft->player->yHeadRotO = yawHead;
    minecraft->player->yRot = yawHead;
    minecraft->player->yRotO = yawHead;
    minecraft->player->xRot = pitch;
    minecraft->player->xRotO = pitch;

    glTranslatef(0, minecraft->player->heightOffset, 0);

    EntityRenderDispatcher::instance->playerRotY = 180;
    EntityRenderDispatcher::instance->render(minecraft->player, 0, 0, 0, 0, 1);

    minecraft->player->yBodyRot = oldBodyRot;
    minecraft->player->yBodyRotO = oldBodyRotO;
    minecraft->player->yHeadRot = oldHeadRot;
    minecraft->player->yHeadRotO = oldHeadRotO;
    minecraft->player->yRot = oldYRot;
    minecraft->player->yRotO = oldYRotO;
    minecraft->player->xRot = oldXRot;
    minecraft->player->xRotO = oldXRotO;

    glPopMatrix();

    Lighting::turnOff();

    // Important: wipe player-model depth before GUI slots/items render.
    glDepthMask(true);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_RESCALE_NORMAL);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
#ifdef GL_SCISSOR_TEST
    glDisable(GL_SCISSOR_TEST);
#endif
    glDisable(GL_RESCALE_NORMAL);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 1);
}

void InventoryScreen::buttonClicked(Button* button)
{
    if (button == nullptr)
    {
        return;
    }

    if (button->id == 0)
    {
        minecraft->setScreen(new AchievementScreen(minecraft->stats[minecraft->player->GetXboxPad()]));
        return;
    }

    if (button->id == 1)
    {
        minecraft->setScreen(new StatsScreen(this, minecraft->stats[minecraft->player->GetXboxPad()]));
        return;
    }
}