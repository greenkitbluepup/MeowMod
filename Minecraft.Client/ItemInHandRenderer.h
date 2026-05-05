#pragma once

class Minecraft;
class ItemInstance;
class Minimap;
class LivingEntity;
class TileRenderer;
class Tesselator;
class Icon;
class ResourceLocation;

class ItemInHandRenderer
{
public:
    // 4J - made these public
    static ResourceLocation ENCHANT_GLINT_LOCATION;
    static ResourceLocation MAP_BACKGROUND_LOCATION;
    static ResourceLocation UNDERWATER_LOCATION;

private:
    Minecraft* minecraft;

    // Main-hand render/equip state
    shared_ptr<ItemInstance> selectedItem;
    float height;
    float oHeight;

    // Offhand render/equip state
    shared_ptr<ItemInstance> m_offhandRenderItem;
    shared_ptr<ItemInstance> m_offhandPendingItem;
    float m_offhandHeight;
    float m_oOffhandHeight;

    // Offhand use/place animation state
    float m_offhandUseAnim;
    float m_oOffhandUseAnim;

    TileRenderer* tileRenderer;

    static int listItem;
    static int listGlint;
    static int listTerrain;

    int lastSlot;

public:
    // 4J Stu - Made public so we can use it from ItemFrameRenderer
    Minimap* minimap;

public:
    ItemInHandRenderer(Minecraft* mc, bool optimisedMinimap = true);

    void renderItem(
        shared_ptr<LivingEntity> mob,
        shared_ptr<ItemInstance> item,
        int layer,
        bool setColor = true,
        bool mirrorSpriteX = false
    );

    static void renderItem3D(
        Tesselator* t,
        float u0,
        float v0,
        float u1,
        float v1,
        int width,
        int height,
        float depth,
        bool isGlint,
        bool isTerrain
    );

    void render(float a);
    void renderScreenEffect(float a);

private:
    void renderTex(float a, Icon* slot);
    void renderWater(float a);
    void renderFire(float a);

public:
    void tick();
    void reset();

    // Main-hand animation hooks
    void itemPlaced();
    void itemUsed();

    // Offhand animation hooks
    void itemPlacedOffhand();
    void itemUsedOffhand();
};