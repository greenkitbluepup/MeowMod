#include "stdafx.h"
#include "AbstractContainerScreen.h"
#include "ItemRenderer.h"
#include "MultiplayerLocalPlayer.h"
#include "Lighting.h"
#include "GameMode.h"
#include "KeyMapping.h"
#include "Options.h"
#include "Tesselator.h"
#ifdef _WINDOWS64
#include "Windows64/KeyboardMouseInput.h"
#endif
#include "..\Minecraft.World\net.minecraft.world.inventory.h"
#include "..\Minecraft.World\net.minecraft.locale.h"
#include "..\Minecraft.World\net.minecraft.world.item.h"

#ifdef _WINDOWS64
extern HWND g_hWnd;
#endif

namespace
{
   void resetFlatGuiState()
    {
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
#ifdef GL_SCISSOR_TEST
        glDisable(GL_SCISSOR_TEST);
#endif
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderManager.StateSetBlendFactor(0xffffffff);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    const wchar_t *getEmptySlotPlaceholderPath(int slotIndex)
    {
       if(slotIndex == InventoryMenu::ARMOR_SLOT_START + 0) return L"gui/item/empty_armor_slot_helmet.png";
        if(slotIndex == InventoryMenu::ARMOR_SLOT_START + 1) return L"gui/item/empty_armor_slot_chestplate.png";
        if(slotIndex == InventoryMenu::ARMOR_SLOT_START + 2) return L"gui/item/empty_armor_slot_leggings.png";
        if(slotIndex == InventoryMenu::ARMOR_SLOT_START + 3) return L"gui/item/empty_armor_slot_boots.png";
        if(slotIndex == InventoryMenu::OFFHAND_SLOT)         return L"gui/item/empty_armor_slot_shield.png";
        return nullptr;
    }

    bool hasPlaceholderTexture(const wchar_t *path)
    {
        if(path == nullptr)
            return false;

        wstring p(path);
        if(app.hasArchiveFile((L"1_2_2/" + p).c_str()))
            return true;
        return app.hasArchiveFile(p.c_str());
    }

    void drawPlaceholderTexture(int x, int y)
    {
        Tesselator *t = Tesselator::getInstance();
        t->begin();
        t->vertexUV(static_cast<float>(x), static_cast<float>(y + 16), 0.0f, 0.0f, 1.0f);
        t->vertexUV(static_cast<float>(x + 16), static_cast<float>(y + 16), 0.0f, 1.0f, 1.0f);
        t->vertexUV(static_cast<float>(x + 16), static_cast<float>(y), 0.0f, 1.0f, 0.0f);
        t->vertexUV(static_cast<float>(x), static_cast<float>(y), 0.0f, 0.0f, 0.0f);
        t->end();
    }

    void getGuiMouseFromRaw(AbstractContainerScreen *screen, int &outX, int &outY)
    {
        outX = 0;
        outY = 0;

        if (screen == nullptr)
            return;

#ifdef _WINDOWS64
        RECT rc;
        GetClientRect(g_hWnd, &rc);
        int clientW = rc.right - rc.left;
        int clientH = rc.bottom - rc.top;
        if (clientW <= 0) clientW = 1;
        if (clientH <= 0) clientH = 1;

        outX = Mouse::getX() * screen->width / clientW;
        outY = screen->height - Mouse::getY() * screen->height / clientH - 1;
#else
        outX = Mouse::getX() * screen->width / screen->minecraft->width;
        outY = screen->height - Mouse::getY() * screen->height / screen->minecraft->height - 1;
#endif
    }
}

ItemRenderer *AbstractContainerScreen::itemRenderer = new ItemRenderer();

AbstractContainerScreen::AbstractContainerScreen(AbstractContainerMenu *menu)
{
	// 4J - added initialisers
	imageWidth = 176;
	imageHeight = 166;
    leftPos = 0;
    topPos = 0;
    m_lastRenderMouseX = 0;
    m_lastRenderMouseY = 0;
    m_lastClickEventMouseX = 0;
    m_lastClickEventMouseY = 0;
    m_liveMouseX = 0;
    m_liveMouseY = 0;
    m_leftClickHandledThisTick = false;
    m_rightClickHandledThisTick = false;

    m_quickCraftDragging = false;
    m_quickCraftMoved = false;
    m_quickCraftButton = -1;
    m_quickCraftStartX = 0;
    m_quickCraftStartY = 0;
    m_quickCraftLastSlotId = -1;
    m_quickCraftMode = QUICK_CRAFT_NONE;
    m_quickCraftSlotIds.clear();

    m_lastInventoryClickTimeMs = 0;
    m_lastInventoryClickButton = -1;
    m_lastInventoryClickSlotId = -1;
    m_lastInventoryClickItemId = -1;
    m_lastInventoryClickItemAux = -1;

    this->menu = menu;
}

void AbstractContainerScreen::init()
{
    Screen::init();
    minecraft->player->containerMenu = menu;
 leftPos = (width - imageWidth) / 2;
    topPos = (height - imageHeight) / 2;

}

void AbstractContainerScreen::render(int xm, int ym, float a)
{
    pollMouseInputForContainerScreen();

    m_lastRenderMouseX = xm;
    m_lastRenderMouseY = ym;

    int mx = xm;
    int my = ym;
    getGuiMouseFromRaw(this, mx, my);
    m_liveMouseX = mx;
    m_liveMouseY = my;

    renderBackground();
    leftPos = (width - imageWidth) / 2;
    topPos = (height - imageHeight) / 2;

    glPushMatrix();
    glTranslatef((float)leftPos, (float)topPos, 0);

    resetFlatGuiState();

    // Background and slots share the same translated inventory-local origin.
    renderBg(a);

    resetFlatGuiState();

    glColor4f(1, 1, 1, 1);
    glEnable(GL_RESCALE_NORMAL);

    Slot *hoveredSlot = nullptr;

   for ( Slot *slot : menu->slots )
	{
       if(slot == nullptr || !slot->isActive())
            continue;

        renderSlot(slot);

        if (isHovering(slot, mx, my))
		{
            hoveredSlot = slot;

            glDisable(GL_LIGHTING);
            glDisable(GL_DEPTH_TEST);

            int x = slot->x;
            int y = slot->y;
            fillGradient(x, y, x + 16, y + 16, 0x80ffffff, 0x80ffffff);
            resetFlatGuiState();
        }
    }

    if (minecraft != nullptr && minecraft->options != nullptr && minecraft->options->renderDebug)
    {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);

        for (Slot *slot : menu->slots)
        {
            if (slot == nullptr || !slot->isActive())
                continue;

            int x = slot->x;
            int y = slot->y;

            fill(x - 1, y - 1, x + 17, y, 0xFFFF0000);
            fill(x - 1, y + 16, x + 17, y + 17, 0xFFFF0000);
            fill(x - 1, y - 1, x, y + 17, 0xFFFF0000);
            fill(x + 16, y - 1, x + 17, y + 17, 0xFFFF0000);
        }

        int localMouseX = mx - leftPos;
        int localMouseY = my - topPos;
        fill(localMouseX - 2, localMouseY - 2, localMouseX + 2, localMouseY + 2, 0xFF0000FF);

        glEnable(GL_DEPTH_TEST);
        resetFlatGuiState();
    }

    renderQuickCraftPreview();

    shared_ptr<Inventory> inventory = minecraft->player->inventory;
    if (inventory->getCarried() != nullptr)
	{
        const int carriedX = mx - leftPos - 8;
        const int carriedY = my - topPos - 8;

        glPushMatrix();

        // Keep carried item above the menu.
        glTranslatef(0.0f, 0.0f, 232.0f);

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderManager.StateSetBlendFactor(0xffffffff);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(true);
        glEnable(GL_RESCALE_NORMAL);
        glEnable(GL_COLOR_MATERIAL);

        // Same localized old-style GUI block lighting used by slot items.
        glPushMatrix();
        glRotatef(120.0f, 1.0f, 0.0f, 0.0f);
        Lighting::turnOn();
        glPopMatrix();

        itemRenderer->renderGuiItem(font, minecraft->textures, inventory->getCarried(), carriedX, carriedY);

        Lighting::turnOff();

        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_RESCALE_NORMAL);

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderManager.StateSetBlendFactor(0xffffffff);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        // Decorations must stay flat.
        itemRenderer->renderGuiItemDecorations(font, minecraft->textures, inventory->getCarried(), carriedX, carriedY);

        glPopMatrix();

        // Hard reset after carried item render.
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_RESCALE_NORMAL);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderManager.StateSetBlendFactor(0xffffffff);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    glDisable(GL_RESCALE_NORMAL);
    resetFlatGuiState();

    renderLabels();

    if (inventory->getCarried() == nullptr && hoveredSlot != nullptr && hoveredSlot->hasItem())
	{
        wstring elementName = hoveredSlot->getItem()->getHoverName();

        if (elementName.length() > 0)
		{
            int x = mx - leftPos + 12;
            int y = my - topPos - 12;
            int width = font->width(elementName);
            fillGradient(x - 3, y - 3, x + width + 3, y + 8 + 3, 0xc0000000, 0xc0000000);

            font->drawShadow(elementName, x, y, 0xffffffff);
        }

    }

    glPopMatrix();

    if (minecraft != nullptr && minecraft->options != nullptr && minecraft->options->renderDebug)
    {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);

        auto drawDot = [](int x, int y, int color)
        {
            Tesselator* t = Tesselator::getInstance();
            t->begin();
            t->color(color);
            t->vertex(static_cast<float>(x - 2), static_cast<float>(y - 2), 0.0f);
            t->vertex(static_cast<float>(x - 2), static_cast<float>(y + 2), 0.0f);
            t->vertex(static_cast<float>(x + 2), static_cast<float>(y + 2), 0.0f);
            t->vertex(static_cast<float>(x + 2), static_cast<float>(y - 2), 0.0f);
            t->end();
        };

        // Red = click event coords, Blue = render() coords.
        drawDot(m_lastClickEventMouseX, m_lastClickEventMouseY, 0xFF0000);
        drawDot(m_lastRenderMouseX, m_lastRenderMouseY, 0x0000FF);

#ifdef _WINDOWS64
        // Green = raw Mouse::getX/getY converted manually into Screen GUI coords.
        extern HWND g_hWnd;
        RECT rc;
        GetClientRect(g_hWnd, &rc);
        int clientW = max(1, rc.right - rc.left);
        int clientH = max(1, rc.bottom - rc.top);
        int gx = Mouse::getX() * width / clientW;
        int gy = height - Mouse::getY() * height / clientH - 1;
        drawDot(gx, gy, 0x00FF00);
#endif

        glEnable(GL_DEPTH_TEST);
      resetFlatGuiState();
    }

    Screen::render(xm, ym, a);
  resetFlatGuiState();
}

shared_ptr<ItemInstance> AbstractContainerScreen::getCarriedStack() const
{
    if (minecraft == nullptr ||
        minecraft->player == nullptr ||
        minecraft->player->inventory == nullptr)
    {
        return nullptr;
    }

    return minecraft->player->inventory->getCarried();
}

bool AbstractContainerScreen::hasCarriedStack() const
{
    shared_ptr<ItemInstance> carried = getCarriedStack();
    return carried != nullptr && carried->count > 0;
}

bool AbstractContainerScreen::quickCraftSlotAlreadyAdded(int slotId) const
{
    for (size_t i = 0; i < m_quickCraftSlotIds.size(); ++i)
    {
        if (m_quickCraftSlotIds[i] == slotId)
        {
            return true;
        }
    }

    return false;
}

bool AbstractContainerScreen::canQuickCraftIntoSlot(Slot* slot, shared_ptr<ItemInstance> carried) const
{
    if (slot == nullptr || carried == nullptr || carried->count <= 0)
    {
        return false;
    }

    if (!slot->isActive())
    {
        return false;
    }

    shared_ptr<ItemInstance> existing = slot->getItem();

    if (existing == nullptr || existing->count <= 0)
    {
        return true;
    }

    if (existing->id != carried->id)
    {
        return false;
    }

    if (existing->getAuxValue() != carried->getAuxValue())
    {
        return false;
    }

    int maxStack = carried->getMaxStackSize();

    int slotMax = slot->getMaxStackSize();
    if (slotMax < maxStack)
    {
        maxStack = slotMax;
    }

    return existing->count < maxStack;
}

bool AbstractContainerScreen::sameItemForCollect(shared_ptr<ItemInstance> a, shared_ptr<ItemInstance> b) const
{
    if (a == nullptr || b == nullptr)
    {
        return false;
    }

    return a->id == b->id &&
        a->getAuxValue() == b->getAuxValue();
}

bool AbstractContainerScreen::tryCollectMatchingItemsIntoCarried(Slot* clickedSlot)
{
    if (clickedSlot == nullptr ||
        minecraft == nullptr ||
        minecraft->player == nullptr ||
        minecraft->player->inventory == nullptr)
    {
        return false;
    }

    shared_ptr<Inventory> inv = minecraft->player->inventory;
    shared_ptr<ItemInstance> carried = inv->getCarried();

    if (carried == nullptr || carried->count <= 0)
    {
        return false;
    }

    const int maxStack = carried->getMaxStackSize();

    if (carried->count >= maxStack)
    {
        return false;
    }

    bool collectedAny = false;

    auto tryPullFromStack = [&](shared_ptr<ItemInstance>& stack) -> void
    {
        if (stack == nullptr || stack->count <= 0)
        {
            return;
        }

        if (!sameItemForCollect(carried, stack))
        {
            return;
        }

        int space = maxStack - carried->count;

        if (space <= 0)
        {
            return;
        }

        int toTake = stack->count;

        if (toTake > space)
        {
            toTake = space;
        }

        if (toTake <= 0)
        {
            return;
        }

        carried->count += toTake;
        stack->count -= toTake;

        if (stack->count <= 0)
        {
            stack = nullptr;
        }

        collectedAny = true;
    };

    // Main inventory backing store.
    // items[0-8] = hotbar, items[9-35] = main inventory rows.
    for (int i = 0; i < (int)inv->items.length; ++i)
    {
        if (carried->count >= maxStack)
        {
            break;
        }

        tryPullFromStack(inv->items[i]);
    }

    // Offhand backing store.
    if (carried->count < maxStack)
    {
        tryPullFromStack(inv->offhand);
    }

    // Armor backing store.
    for (int i = 0; i < (int)inv->armor.length; ++i)
    {
        if (carried->count >= maxStack)
        {
            break;
        }

        tryPullFromStack(inv->armor[i]);
    }

    if (collectedAny)
    {
        inv->setCarried(carried);
        inv->setChanged();

        if (menu != nullptr)
        {
            menu->slotsChanged();
        }
    }

    return collectedAny;
}

int AbstractContainerScreen::getQuickCraftPreviewCountForSlot(int targetSlotId) const
{
    if (!m_quickCraftDragging ||
        m_quickCraftMode != QUICK_CRAFT_LEFT_SPLIT ||
        targetSlotId < 0)
    {
        return 0;
    }

    shared_ptr<ItemInstance> carried = getCarriedStack();
    if (carried == nullptr || carried->count <= 0)
    {
        return 0;
    }

    int validCount = 0;

    for (size_t i = 0; i < m_quickCraftSlotIds.size(); ++i)
    {
        int slotId = m_quickCraftSlotIds[i];

        if (menu == nullptr ||
            slotId < 0 ||
            slotId >= static_cast<int>(menu->slots.size()))
        {
            continue;
        }

        Slot* slot = menu->slots[slotId];

        if (canQuickCraftIntoSlot(slot, carried))
        {
            validCount++;
        }
    }

    if (validCount <= 0)
    {
        return 0;
    }

    int perSlot = carried->count / validCount;

    if (perSlot <= 0)
    {
        perSlot = 1;
    }

    int remaining = carried->count;

    for (size_t i = 0; i < m_quickCraftSlotIds.size(); ++i)
    {
        int slotId = m_quickCraftSlotIds[i];

        if (menu == nullptr ||
            slotId < 0 ||
            slotId >= static_cast<int>(menu->slots.size()))
        {
            continue;
        }

        Slot* slot = menu->slots[slotId];

        if (!canQuickCraftIntoSlot(slot, carried))
        {
            continue;
        }

        shared_ptr<ItemInstance> existing = slot->getItem();

        int maxStack = carried->getMaxStackSize();
        int slotMax = slot->getMaxStackSize();

        if (slotMax < maxStack)
        {
            maxStack = slotMax;
        }

        int existingCount = existing != nullptr ? existing->count : 0;
        int space = maxStack - existingCount;

        if (space <= 0)
        {
            continue;
        }

        int toPlace = perSlot;

        if (toPlace > remaining)
        {
            toPlace = remaining;
        }

        if (toPlace > space)
        {
            toPlace = space;
        }

        if (toPlace < 0)
        {
            toPlace = 0;
        }

        if (slotId == targetSlotId)
        {
            return toPlace;
        }

        remaining -= toPlace;

        if (remaining <= 0)
        {
            break;
        }
    }

    return 0;
}

void AbstractContainerScreen::renderQuickCraftPreview()
{
    if (!m_quickCraftDragging ||
        m_quickCraftMode != QUICK_CRAFT_LEFT_SPLIT ||
        !m_quickCraftMoved ||
        m_quickCraftSlotIds.size() < 2)
    {
        return;
    }

    shared_ptr<ItemInstance> carried = getCarriedStack();

    if (carried == nullptr || carried->count <= 0)
    {
        return;
    }

    for (size_t i = 0; i < m_quickCraftSlotIds.size(); ++i)
    {
        int slotId = m_quickCraftSlotIds[i];

        if (menu == nullptr ||
            slotId < 0 ||
            slotId >= static_cast<int>(menu->slots.size()))
        {
            continue;
        }

        Slot* slot = menu->slots[slotId];

        if (slot == nullptr || !slot->isActive())
        {
            continue;
        }

        int previewAdd = getQuickCraftPreviewCountForSlot(slotId);

        if (previewAdd <= 0)
        {
            continue;
        }

        shared_ptr<ItemInstance> previewItem = carried->copy();

        shared_ptr<ItemInstance> existing = slot->getItem();
        if (existing != nullptr &&
            existing->id == carried->id &&
            existing->getAuxValue() == carried->getAuxValue())
        {
            previewItem->count = existing->count + previewAdd;
        }
        else
        {
            previewItem->count = previewAdd;
        }

        int maxStack = previewItem->getMaxStackSize();
        int slotMax = slot->getMaxStackSize();

        if (slotMax < maxStack)
        {
            maxStack = slotMax;
        }

        if (previewItem->count > maxStack)
        {
            previewItem->count = maxStack;
        }

        int x = slot->x;
        int y = slot->y;

        glPushMatrix();

        glTranslatef(0.0f, 0.0f, 220.0f);

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderManager.StateSetBlendFactor(0xffffffff);
        glColor4f(1.0f, 1.0f, 1.0f, 0.75f);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(true);
        glEnable(GL_RESCALE_NORMAL);
        glEnable(GL_COLOR_MATERIAL);

        glPushMatrix();
        glRotatef(120.0f, 1.0f, 0.0f, 0.0f);
        Lighting::turnOn();
        glPopMatrix();

        itemRenderer->renderGuiItem(font, minecraft->textures, previewItem, x, y);

        Lighting::turnOff();

        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_RESCALE_NORMAL);

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderManager.StateSetBlendFactor(0xffffffff);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        itemRenderer->renderGuiItemDecorations(font, minecraft->textures, previewItem, x, y);

        glPopMatrix();

        resetFlatGuiState();

        fillGradient(x, y, x + 16, y + 16, 0x40ffffff, 0x40ffffff);
        resetFlatGuiState();
    }
}

void AbstractContainerScreen::beginQuickCraftDrag(int mx, int my, int buttonNum)
{
    cancelQuickCraftDrag();

    if (!hasCarriedStack())
    {
        return;
    }

    if (buttonNum == 0)
    {
        m_quickCraftMode = QUICK_CRAFT_LEFT_SPLIT;
    }
    else if (buttonNum == 1)
    {
        m_quickCraftMode = QUICK_CRAFT_RIGHT_ONE;
    }
    else
    {
        m_quickCraftMode = QUICK_CRAFT_NONE;
        return;
    }

    m_quickCraftDragging = true;
    m_quickCraftMoved = false;
    m_quickCraftButton = buttonNum;
    m_quickCraftStartX = mx;
    m_quickCraftStartY = my;
    m_quickCraftLastSlotId = -1;
    m_quickCraftSlotIds.clear();

    updateQuickCraftDrag(mx, my);
}

void AbstractContainerScreen::updateQuickCraftDrag(int mx, int my)
{
    if (!m_quickCraftDragging)
    {
        return;
    }

    int dx = mx - m_quickCraftStartX;
    int dy = my - m_quickCraftStartY;

    if (dx * dx + dy * dy > 4)
    {
        m_quickCraftMoved = true;
    }

    Slot* slot = findSlot(mx, my);
    if (slot == nullptr)
    {
        return;
    }

    int slotId = slot->index;

    if (slotId == m_quickCraftLastSlotId)
    {
        return;
    }

    m_quickCraftLastSlotId = slotId;

    if (quickCraftSlotAlreadyAdded(slotId))
    {
        return;
    }

    shared_ptr<ItemInstance> carried = getCarriedStack();

    if (!canQuickCraftIntoSlot(slot, carried))
    {
        return;
    }

    m_quickCraftSlotIds.push_back(slotId);

    // Right-drag updates immediately, like Java.
    if (m_quickCraftMode == QUICK_CRAFT_RIGHT_ONE)
    {
        int clickX = leftPos + slot->x + 8;
        int clickY = topPos + slot->y + 8;

        handleContainerClickAt(clickX, clickY, 1);
    }

    // Left-drag is previewed live, but committed on release.
}

void AbstractContainerScreen::finishQuickCraftDrag(int mx, int my)
{
    if (!m_quickCraftDragging)
    {
        return;
    }

    // RIGHT-drag already placed items live during updateQuickCraftDrag().
    // Do not apply again on release, or it will double-place.
    if (m_quickCraftMode == QUICK_CRAFT_RIGHT_ONE)
    {
        // If the mouse never actually moved, this was just a normal right click.
        // But updateQuickCraftDrag() already handled the starting slot, so no extra click here.
        cancelQuickCraftDrag();
        return;
    }

    // LEFT-drag split still applies on release.
    bool shouldQuickCraft =
        m_quickCraftMoved &&
        m_quickCraftSlotIds.size() >= 2;

    if (shouldQuickCraft)
    {
        applyQuickCraftDrag();
    }
    else
    {
        // Less than 2 slots means it was just a normal click.
        handleContainerClickAt(m_quickCraftStartX, m_quickCraftStartY, m_quickCraftButton);
    }

    cancelQuickCraftDrag();
}

void AbstractContainerScreen::cancelQuickCraftDrag()
{
    m_quickCraftDragging = false;
    m_quickCraftMoved = false;
    m_quickCraftButton = -1;
    m_quickCraftStartX = 0;
    m_quickCraftStartY = 0;
    m_quickCraftLastSlotId = -1;
    m_quickCraftMode = QUICK_CRAFT_NONE;
    m_quickCraftSlotIds.clear();
}

void AbstractContainerScreen::applyQuickCraftDrag()
{
    shared_ptr<ItemInstance> carried = getCarriedStack();

    if (carried == nullptr || carried->count <= 0)
    {
        return;
    }

    if (m_quickCraftSlotIds.empty())
    {
        return;
    }

    // Right-drag behavior:
    // one right-click per dragged slot.
    // Existing inventory click logic already places one item from the carried stack.
    if (m_quickCraftMode == QUICK_CRAFT_RIGHT_ONE)
    {
        for (size_t i = 0; i < m_quickCraftSlotIds.size(); ++i)
        {
            carried = getCarriedStack();
            if (carried == nullptr || carried->count <= 0)
            {
                break;
            }

            int slotId = m_quickCraftSlotIds[i];

            if (menu == nullptr ||
                slotId < 0 ||
                slotId >= static_cast<int>(menu->slots.size()))
            {
                continue;
            }

            Slot* slot = menu->slots[slotId];

            if (!canQuickCraftIntoSlot(slot, carried))
            {
                continue;
            }

            int clickX = leftPos + slot->x + 8;
            int clickY = topPos + slot->y + 8;

            handleContainerClickAt(clickX, clickY, 1);
        }

        return;
    }

    // Left-drag behavior:
    // split the carried stack evenly across dragged valid slots.
    // This replays right-clicks so we reuse the existing transaction system.
    if (m_quickCraftMode == QUICK_CRAFT_LEFT_SPLIT)
    {
        int validCount = 0;

        for (size_t i = 0; i < m_quickCraftSlotIds.size(); ++i)
        {
            int slotId = m_quickCraftSlotIds[i];

            if (menu == nullptr ||
                slotId < 0 ||
                slotId >= static_cast<int>(menu->slots.size()))
            {
                continue;
            }

            Slot* slot = menu->slots[slotId];

            if (canQuickCraftIntoSlot(slot, carried))
            {
                validCount++;
            }
        }

        if (validCount <= 0)
        {
            return;
        }

        int perSlot = carried->count / validCount;

        // If the player drags fewer items than slots, place one into as many slots as possible.
        if (perSlot <= 0)
        {
            perSlot = 1;
        }

        for (size_t i = 0; i < m_quickCraftSlotIds.size(); ++i)
        {
            carried = getCarriedStack();
            if (carried == nullptr || carried->count <= 0)
            {
                break;
            }

            int slotId = m_quickCraftSlotIds[i];

            if (menu == nullptr ||
                slotId < 0 ||
                slotId >= static_cast<int>(menu->slots.size()))
            {
                continue;
            }

            Slot* slot = menu->slots[slotId];

            if (!canQuickCraftIntoSlot(slot, carried))
            {
                continue;
            }

            int clickX = leftPos + slot->x + 8;
            int clickY = topPos + slot->y + 8;

            for (int n = 0; n < perSlot; ++n)
            {
                carried = getCarriedStack();
                if (carried == nullptr || carried->count <= 0)
                {
                    return;
                }

                if (!canQuickCraftIntoSlot(slot, carried))
                {
                    break;
                }

                // Existing right-click transaction places one item.
                handleContainerClickAt(clickX, clickY, 1);
            }
        }
    }
}

void AbstractContainerScreen::pollMouseInputForContainerScreen()
{
#ifdef _WINDOWS64
    RECT rc;
    GetClientRect(g_hWnd, &rc);

    int clientW = max(1, rc.right - rc.left);
    int clientH = max(1, rc.bottom - rc.top);

    int mx = Mouse::getX() * width / clientW;
    int my = height - Mouse::getY() * height / clientH - 1;

    m_liveMouseX = mx;
    m_liveMouseY = my;

    if (m_quickCraftDragging)
    {
        updateQuickCraftDrag(mx, my);
    }

    if (g_KBMInput.IsMouseButtonPressed(KeyboardMouseInput::MOUSE_LEFT) && !m_leftClickHandledThisTick)
    {
        m_leftClickHandledThisTick = true;

        Slot* clickSlot = findSlot(mx, my);

        if (clickSlot != nullptr && clickSlot->isActive())
        {
            long long now = System::currentTimeMillis();

            shared_ptr<ItemInstance> carried = getCarriedStack();

            bool isDoubleClick =
                m_lastInventoryClickButton == 0 &&
                m_lastInventoryClickSlotId == clickSlot->index &&
                now - m_lastInventoryClickTimeMs <= 300;

            // Second click:
            // First click picked up the visible stack.
            // Second click, same slot, gathers matching stacks into the carried stack.
            if (isDoubleClick &&
                m_lastInventoryClickItemId >= 0 &&
                carried != nullptr &&
                carried->count > 0 &&
                carried->id == m_lastInventoryClickItemId &&
                carried->getAuxValue() == m_lastInventoryClickItemAux)
            {
                if (tryCollectMatchingItemsIntoCarried(clickSlot))
                {
                    // Notify the server so its slot state matches what we just gathered.
                    // Without this, broadcastChanges() will push stale server counts back.
                    minecraft->gameMode->handleInventoryPickupAll(
                        menu->containerId, clickSlot->index, minecraft->player);

                    m_lastInventoryClickTimeMs = 0;
                    m_lastInventoryClickButton = -1;
                    m_lastInventoryClickSlotId = -1;
                    m_lastInventoryClickItemId = -1;
                    m_lastInventoryClickItemAux = -1;

                    cancelQuickCraftDrag();
                    return;
                }
            }

            // First click:
            // Record the visible item BEFORE normal click picks it up.
            shared_ptr<ItemInstance> slotItem = clickSlot->getItem();

            m_lastInventoryClickTimeMs = now;
            m_lastInventoryClickButton = 0;
            m_lastInventoryClickSlotId = clickSlot->index;

            if (slotItem != nullptr && slotItem->count > 0)
            {
                m_lastInventoryClickItemId = slotItem->id;
                m_lastInventoryClickItemAux = slotItem->getAuxValue();
            }
        }

        if (hasCarriedStack())
        {
            beginQuickCraftDrag(mx, my, 0);
        }
        else
        {
            mouseClicked(mx, my, 0);
        }
    }

    if (g_KBMInput.IsMouseButtonPressed(KeyboardMouseInput::MOUSE_RIGHT) && !m_rightClickHandledThisTick)
    {
        m_rightClickHandledThisTick = true;

        if (hasCarriedStack())
        {
            beginQuickCraftDrag(mx, my, 1);
        }
        else
        {
            mouseClicked(mx, my, 1);
        }
    }

    if (g_KBMInput.IsMouseButtonReleased(KeyboardMouseInput::MOUSE_LEFT))
    {
        if (m_quickCraftDragging && m_quickCraftButton == 0)
        {
            finishQuickCraftDrag(mx, my);
        }
        else
        {
            mouseReleased(mx, my, 0);
        }
    }

    if (g_KBMInput.IsMouseButtonReleased(KeyboardMouseInput::MOUSE_RIGHT))
    {
        if (m_quickCraftDragging && m_quickCraftButton == 1)
        {
            finishQuickCraftDrag(mx, my);
        }
        else
        {
            mouseReleased(mx, my, 1);
        }
    }
#endif
}

void AbstractContainerScreen::renderLabels()
{
}

void AbstractContainerScreen::renderSlot(Slot *slot)
{
    if(slot == nullptr || !slot->isActive())
        return;

    int x = slot->x;
    int y = slot->y;
    shared_ptr<ItemInstance> item = slot->getItem();

    if (item == nullptr)
	{
     const wchar_t *placeholderPath = getEmptySlotPlaceholderPath(slot->index);
        if(hasPlaceholderTexture(placeholderPath))
        {
            resetFlatGuiState();
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            minecraft->textures->bindTexture(placeholderPath);
            drawPlaceholderTexture(x, y);
            resetFlatGuiState();
        }
        return;
    }

    glPushMatrix();
#ifdef GL_SCISSOR_TEST
    glDisable(GL_SCISSOR_TEST);
#endif
 glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    RenderManager.StateSetBlendFactor(0xffffffff);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(true);
    glEnable(GL_RESCALE_NORMAL);
    glEnable(GL_COLOR_MATERIAL);

    glPushMatrix();
    glRotatef(120.0f, 1.0f, 0.0f, 0.0f);
    Lighting::turnOn();
    glPopMatrix();

    itemRenderer->renderGuiItem(font, minecraft->textures, item, x, y);

 Lighting::turnOff();
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
  glDisable(GL_RESCALE_NORMAL);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    RenderManager.StateSetBlendFactor(0xffffffff);
    itemRenderer->renderGuiItemDecorations(font, minecraft->textures, item, x, y);

    glPopMatrix();
}

Slot *AbstractContainerScreen::findSlot(int x, int y)
{
    for (Slot* slot : menu->slots )
	{
        if(slot == nullptr || !slot->isActive())
            continue;

        if (isHovering(slot, x, y)) return slot;
    }
    return nullptr;
}

bool AbstractContainerScreen::isHovering(Slot *slot, int xm, int ym)
{
    xm -= leftPos;
    ym -= topPos;

    return xm >= slot->x - 1 && xm < slot->x + 16 + 1 && ym >= slot->y - 1 && ym < slot->y + 16 + 1;

}

void AbstractContainerScreen::mouseClicked(int x, int y, int buttonNum)
{
  // Disabled while testing inventory transaction path; this screen currently has
    // no active button widgets and we don't want base button handling to interfere.
    //Screen::mouseClicked(x, y, buttonNum);
    m_lastClickEventMouseX = x;
    m_lastClickEventMouseY = y;

    int mx = x;
    int my = y;

    if(buttonNum == 0) m_leftClickHandledThisTick = true;
    if(buttonNum == 1) m_rightClickHandledThisTick = true;

    handleContainerClickAt(mx, my, buttonNum);
}

void AbstractContainerScreen::handleContainerClickAt(int mx, int my, int buttonNum)
{
    if (buttonNum != 0 && buttonNum != 1)
        return;

    Slot *slot = findSlot(mx, my);

    bool clickedOutside = (mx < leftPos || my < topPos || mx >= leftPos + imageWidth || my >= topPos + imageHeight);

    int slotId = -1;
    if (slot != nullptr) slotId = slot->index;

    shared_ptr<ItemInstance> beforeSlotItem = nullptr;
    if (slot != nullptr)
        beforeSlotItem = slot->getItem();

    shared_ptr<ItemInstance> beforeCarried = minecraft->player->inventory->getCarried();

    app.DebugPrintf(
        "[INV_CLICK_PRE] button=%d mx=%d my=%d slotId=%d slotItem=%d:%d carried=%d:%d containerId=%d playerContainerId=%d\n",
        buttonNum,
        mx,
        my,
        slotId,
        beforeSlotItem ? beforeSlotItem->id : -1,
        beforeSlotItem ? beforeSlotItem->count : 0,
        beforeCarried ? beforeCarried->id : -1,
        beforeCarried ? beforeCarried->count : 0,
        menu ? menu->containerId : -999,
        (minecraft->player && minecraft->player->containerMenu) ? minecraft->player->containerMenu->containerId : -999
    );
    app.DebugPrintf(app.USER_SR,
        "[INV_CLICK_PRE] button=%d mx=%d my=%d slotId=%d slotItem=%d:%d carried=%d:%d containerId=%d playerContainerId=%d\n",
        buttonNum,
        mx,
        my,
        slotId,
        beforeSlotItem ? beforeSlotItem->id : -1,
        beforeSlotItem ? beforeSlotItem->count : 0,
        beforeCarried ? beforeCarried->id : -1,
        beforeCarried ? beforeCarried->count : 0,
        menu ? menu->containerId : -999,
        (minecraft->player && minecraft->player->containerMenu) ? minecraft->player->containerMenu->containerId : -999
    );

    app.DebugPrintf(
        "[INV_MOUSE] event=%d,%d render=%d,%d live=%d,%d screen=%d,%d mc=%d,%d phys=%d,%d image=%d,%d origin=%d,%d slot=%d outside=%d\n",
        m_lastClickEventMouseX, m_lastClickEventMouseY,
        m_lastRenderMouseX, m_lastRenderMouseY,
        m_liveMouseX, m_liveMouseY,
        width, height,
        minecraft ? minecraft->width : 0,
        minecraft ? minecraft->height : 0,
        minecraft ? minecraft->width_phys : 0,
        minecraft ? minecraft->height_phys : 0,
        imageWidth, imageHeight,
        leftPos, topPos,
        slotId,
        clickedOutside ? 1 : 0
    );

    if (clickedOutside)
    {
        slotId = AbstractContainerMenu::SLOT_CLICKED_OUTSIDE;
    }

    if (slotId == -1)
    {
        return;
    }

    

    bool quickKey = slotId != AbstractContainerMenu::SLOT_CLICKED_OUTSIDE &&
        (Keyboard::isKeyDown(Keyboard::KEY_LSHIFT) || Keyboard::isKeyDown(Keyboard::KEY_RSHIFT));

    minecraft->gameMode->handleInventoryMouseClick(menu->containerId, slotId, buttonNum, quickKey, minecraft->player);

    shared_ptr<ItemInstance> afterSlotItem = nullptr;
    if (slot != nullptr)
        afterSlotItem = slot->getItem();

    shared_ptr<ItemInstance> afterCarried = minecraft->player->inventory->getCarried();

    app.DebugPrintf(
        "[INV_CLICK_POST] slotId=%d slotItem=%d:%d carried=%d:%d\n",
        slotId,
        afterSlotItem ? afterSlotItem->id : -1,
        afterSlotItem ? afterSlotItem->count : 0,
        afterCarried ? afterCarried->id : -1,
        afterCarried ? afterCarried->count : 0
    );
    app.DebugPrintf(app.USER_SR,
        "[INV_CLICK_POST] slotId=%d slotItem=%d:%d carried=%d:%d\n",
        slotId,
        afterSlotItem ? afterSlotItem->id : -1,
        afterSlotItem ? afterSlotItem->count : 0,
        afterCarried ? afterCarried->id : -1,
        afterCarried ? afterCarried->count : 0
    );
}

void AbstractContainerScreen::mouseReleased(int x, int y, int buttonNum)
{
    if (buttonNum == 0)
	{
    }
}

void AbstractContainerScreen::keyPressed(wchar_t eventCharacter, int eventKey)
{
    if (eventKey == Keyboard::KEY_ESCAPE || eventKey == minecraft->options->keyBuild->key)
	{
        minecraft->player->closeContainer();
       minecraft->setScreen(nullptr);
        return;
    }
}

void AbstractContainerScreen::removed()
{
    if (minecraft->player == nullptr) return;
}

void AbstractContainerScreen::slotsChanged(shared_ptr<Container> container)
{
}

bool AbstractContainerScreen::isPauseScreen()
{
	return false;
}

void AbstractContainerScreen::tick()
{
    m_leftClickHandledThisTick = false;
    m_rightClickHandledThisTick = false;

    Screen::tick();

    if (!minecraft->player->isAlive() || minecraft->player->removed)
    {
        minecraft->player->closeContainer();
    }
}