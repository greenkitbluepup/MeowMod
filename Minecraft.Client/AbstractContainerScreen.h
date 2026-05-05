#pragma once
#include "Screen.h"
#include <vector>

class ItemRenderer;
class AbstractContainerMenu;
class Slot;
class Container;
class ItemInstance;

class AbstractContainerScreen : public Screen
{
private:
    static ItemRenderer* itemRenderer;

protected:
    int imageWidth;
    int imageHeight;
    int leftPos;
    int topPos;

    int m_lastRenderMouseX;
    int m_lastRenderMouseY;
    int m_lastClickEventMouseX;
    int m_lastClickEventMouseY;
    int m_liveMouseX;
    int m_liveMouseY;

    bool m_leftClickHandledThisTick;
    bool m_rightClickHandledThisTick;

    enum ContainerQuickCraftMode
    {
        QUICK_CRAFT_NONE = 0,
        QUICK_CRAFT_LEFT_SPLIT = 1,
        QUICK_CRAFT_RIGHT_ONE = 2
    };

    bool m_quickCraftDragging;
    bool m_quickCraftMoved;
    int m_quickCraftButton;
    int m_quickCraftStartX;
    int m_quickCraftStartY;
    int m_quickCraftLastSlotId;
    ContainerQuickCraftMode m_quickCraftMode;
    std::vector<int> m_quickCraftSlotIds;

    // Double-click collect tracking.
    long long m_lastInventoryClickTimeMs;
    int m_lastInventoryClickButton;
    int m_lastInventoryClickSlotId;
    int m_lastInventoryClickItemId;
    int m_lastInventoryClickItemAux;

public:
    AbstractContainerMenu* menu;

    int getLiveMouseX() const { return m_liveMouseX; }
    int getLiveMouseY() const { return m_liveMouseY; }

    AbstractContainerScreen(AbstractContainerMenu* menu);
    virtual void init();
    virtual void render(int xm, int ym, float a);

protected:
    virtual void renderLabels();
    virtual void renderBg(float a) = 0;

private:
    virtual void renderSlot(Slot* slot);
    virtual Slot* findSlot(int x, int y);
    virtual bool isHovering(Slot* slot, int xm, int ym);

    shared_ptr<ItemInstance> getCarriedStack() const;
    bool hasCarriedStack() const;
    bool canQuickCraftIntoSlot(Slot* slot, shared_ptr<ItemInstance> carried) const;
    bool quickCraftSlotAlreadyAdded(int slotId) const;

    int getQuickCraftPreviewCountForSlot(int slotId) const;
    void renderQuickCraftPreview();

    bool sameItemForCollect(shared_ptr<ItemInstance> a, shared_ptr<ItemInstance> b) const;
    bool tryCollectMatchingItemsIntoCarried(Slot* clickedSlot);

    void beginQuickCraftDrag(int mx, int my, int buttonNum);
    void updateQuickCraftDrag(int mx, int my);
    void finishQuickCraftDrag(int mx, int my);
    void cancelQuickCraftDrag();
    void applyQuickCraftDrag();

protected:
    void handleContainerClickAt(int mx, int my, int buttonNum);
    void pollMouseInputForContainerScreen();

    virtual void mouseClicked(int x, int y, int buttonNum);
    virtual void mouseReleased(int x, int y, int buttonNum);
    virtual void keyPressed(wchar_t eventCharacter, int eventKey);

public:
    virtual void removed();
    virtual void slotsChanged(shared_ptr<Container> container);
    virtual bool isPauseScreen();
    virtual void tick();
};