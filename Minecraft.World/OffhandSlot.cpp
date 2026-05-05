#include "stdafx.h"
#include "net.minecraft.world.entity.player.h"
#include "OffhandSlot.h"
#include "Inventory.h"

OffhandSlot::OffhandSlot(Inventory* inventory, int x, int y)
    : Slot(std::shared_ptr<Container>(), 0, x, y)
    , m_inventory(inventory)
{
}

shared_ptr<ItemInstance> OffhandSlot::getItem()
{
    return m_inventory->getOffhand();
}

bool OffhandSlot::hasItem()
{
    return m_inventory->getOffhand() != nullptr;
}

void OffhandSlot::set(shared_ptr<ItemInstance> item)
{
    m_inventory->setOffhand(item);
    m_inventory->setChanged();
}

void OffhandSlot::setChanged()
{
    m_inventory->setChanged();
}

shared_ptr<ItemInstance> OffhandSlot::remove(int count)
{
    shared_ptr<ItemInstance> item = m_inventory->getOffhand();
    if (!item)
        return nullptr;

    if (count >= item->count)
    {
        m_inventory->setOffhand(nullptr);
        m_inventory->setChanged();
        return item;
    }

    shared_ptr<ItemInstance> split = item->remove(count);
    if (item->count == 0)
        m_inventory->setOffhand(nullptr);
    m_inventory->setChanged();
    return split;
}

int OffhandSlot::getMaxStackSize() const
{
    return 64;
}
