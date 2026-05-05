#pragma once
#include "Item.h"
#include <string>

// ModItem -- a concrete Item subclass instantiated at runtime for each
// item registered via ModHostAPI::registerItem.
//
// The id parameter passed to Item(id) is (finalGameId - 256) because the
// Item base constructor stores id as (256 + param).  Pass (finalId - 256).
// One instance per registered mod item; never deleted.
class ModItem : public Item
{
public:
    // modItemId : raw parameter for Item(id) constructor -- caller must
    //             subtract 256 from the desired final game ID before passing.
    ModItem(int modItemId,
            const std::wstring& textureName,
            int maxStack,
            int flags); // MOD_ITEM_* bitmask
};
