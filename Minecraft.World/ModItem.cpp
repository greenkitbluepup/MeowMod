#include "stdafx.h"
#include "ModItem.h"

ModItem::ModItem(int modItemId,
                 const std::wstring& textureName,
                 int maxStack,
                 int flags)
    : Item(modItemId)
{
    setIconName(textureName);
    setMaxStackSize(maxStack > 0 ? maxStack : 1);
    (void)flags;
}
