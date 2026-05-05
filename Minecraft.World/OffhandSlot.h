#pragma once
#pragma once

#include "Slot.h"

class Inventory;

// A HUD-read-only slot that exposes Inventory::offhand as a regular Slot.
// getItem/set/remove/setChanged are routed directly to inventory->offhand
// so the offhand field is never confused with any numbered container slot.
class OffhandSlot : public Slot
{
private:
    Inventory* m_inventory;

public:
    // x/y are layout coords for inventory screen (unused on HUD, but required by base).
    OffhandSlot(Inventory* inventory, int x, int y);
    virtual ~OffhandSlot() {}

    virtual shared_ptr<ItemInstance> getItem() override;
    virtual bool                     hasItem()  override;
    virtual void                     set(shared_ptr<ItemInstance> item) override;
    virtual void                     setChanged() override;
    virtual shared_ptr<ItemInstance> remove(int count) override;
    virtual int                      getMaxStackSize() const override;
};
