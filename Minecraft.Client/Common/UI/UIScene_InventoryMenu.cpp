#include "stdafx.h"
#include "UI.h"
#include "UIScene_InventoryMenu.h"

#include "..\..\..\Minecraft.World\net.minecraft.world.inventory.h"
#include "..\..\..\Minecraft.World\net.minecraft.world.item.h"
#include "..\..\..\Minecraft.World\net.minecraft.stats.h"
#include "..\..\..\Minecraft.World\net.minecraft.world.effect.h"
#include "..\..\MultiplayerLocalPlayer.h"
#include "..\..\Minecraft.h"
#include "..\..\Options.h"
#include "..\..\EntityRenderDispatcher.h"
#include "..\..\Lighting.h"
#include "..\Tutorial\Tutorial.h"
#include "..\Tutorial\TutorialMode.h"
#include "..\Tutorial\TutorialEnum.h"

#define INVENTORY_UPDATE_EFFECTS_TIMER_ID (10)
#define INVENTORY_UPDATE_EFFECTS_TIMER_TIME (1000) // 1 second

static void fillSolidRect(float x0, float y0, float x1, float y1, int color)
{
	glDisable(GL_TEXTURE_2D);

	Tesselator *t = Tesselator::getInstance();
	t->begin();
	t->color(color);
	t->vertex(x0, y0, 0.0f);
	t->vertex(x0, y1, 0.0f);
	t->vertex(x1, y1, 0.0f);
	t->vertex(x1, y0, 0.0f);
	t->end();

	glEnable(GL_TEXTURE_2D);
}

UIScene_InventoryMenu::UIScene_InventoryMenu(int iPad, void *_initData, UILayer *parentLayer) : UIScene_AbstractContainerMenu(iPad, parentLayer)
{
	// Setup all the Iggy references we need for this scene
	initialiseMovie();

	InventoryScreenInput *initData = static_cast<InventoryScreenInput *>(_initData);

	Minecraft *pMinecraft = Minecraft::GetInstance();
	if( pMinecraft->localgameModes[initData->iPad] != nullptr )
	{
		TutorialMode *gameMode = static_cast<TutorialMode *>(pMinecraft->localgameModes[initData->iPad]);
		m_previousTutorialState = gameMode->getTutorial()->getCurrentState();
		gameMode->getTutorial()->changeTutorialState(e_Tutorial_State_Inventory_Menu, this);
	}

	InventoryMenu *menu = static_cast<InventoryMenu *>(initData->player->inventoryMenu);

	initData->player->awardStat(GenericStats::openInventory(),GenericStats::param_openInventory());

	Initialize( initData->iPad, menu, false, InventoryMenu::INV_SLOT_START, eSectionInventoryUsing, eSectionInventoryMax, initData->bNavigateBack );

	m_slotListArmor.addSlots(InventoryMenu::ARMOR_SLOT_START, InventoryMenu::ARMOR_SLOT_END - InventoryMenu::ARMOR_SLOT_START);

	if(initData) delete initData;

	for(unsigned int i = 0; i < MobEffect::NUM_EFFECTS; ++i)
	{
		m_bEffectTime[i] = 0;
	}

	updateEffectsDisplay();
	addTimer(INVENTORY_UPDATE_EFFECTS_TIMER_ID,INVENTORY_UPDATE_EFFECTS_TIMER_TIME);
}

wstring UIScene_InventoryMenu::getMoviePath()
{
	if(app.GetLocalPlayerCount() > 1)
	{
		return L"InventoryMenuSplit";
	}
	else
	{
		return L"InventoryMenu";
	}
}

void UIScene_InventoryMenu::handleReload()
{
	Initialize( m_iPad, m_menu, false, InventoryMenu::INV_SLOT_START, eSectionInventoryUsing, eSectionInventoryMax, m_bNavigateBack );

	m_slotListArmor.addSlots(InventoryMenu::ARMOR_SLOT_START, InventoryMenu::ARMOR_SLOT_END - InventoryMenu::ARMOR_SLOT_START);

	for(unsigned int i = 0; i < MobEffect::NUM_EFFECTS; ++i)
	{
		m_bEffectTime[i] = 0;
	}
}

int UIScene_InventoryMenu::getSectionColumns(ESceneSection eSection)
{
	int cols = 0;
	switch( eSection )
	{
	case eSectionInventoryArmor:
		cols = 1;
		break;
	case eSectionInventoryInventory:
		cols = 9;
		break;
	case eSectionInventoryUsing:
		cols = 9;
		break;
	default:
		assert( false );
		break;
	}
	return cols;
}

int UIScene_InventoryMenu::getSectionRows(ESceneSection eSection)
{
	int rows = 0;
	switch( eSection )
	{
	case eSectionInventoryArmor:
		rows = 4;
		break;
	case eSectionInventoryInventory:
		rows = 3;
		break;
	case eSectionInventoryUsing:
		rows = 1;
		break;
	default:
		assert( false );
		break;
	}
	return rows;
}

void UIScene_InventoryMenu::GetPositionOfSection( ESceneSection eSection, UIVec2D* pPosition )
{
	switch( eSection )
	{
	case eSectionInventoryArmor:
		pPosition->x = m_slotListArmor.getXPos();
		pPosition->y = m_slotListArmor.getYPos();
		break;
	case eSectionInventoryInventory:
		pPosition->x = m_slotListInventory.getXPos();
		pPosition->y = m_slotListInventory.getYPos();
		break;
	case eSectionInventoryUsing:
		pPosition->x = m_slotListHotbar.getXPos();
		pPosition->y = m_slotListHotbar.getYPos();
		break;
	default:
		assert( false );
		break;
	}
}

void UIScene_InventoryMenu::GetItemScreenData( ESceneSection eSection, int iItemIndex, UIVec2D* pPosition, UIVec2D* pSize )
{
	UIVec2D sectionSize;

	switch( eSection )
	{
	case eSectionInventoryArmor:
		sectionSize.x = m_slotListArmor.getWidth();
		sectionSize.y = m_slotListArmor.getHeight();
		break;
	case eSectionInventoryInventory:
		sectionSize.x = m_slotListInventory.getWidth();
		sectionSize.y = m_slotListInventory.getHeight();
		break;
	case eSectionInventoryUsing:
		sectionSize.x = m_slotListHotbar.getWidth();
		sectionSize.y = m_slotListHotbar.getHeight();
		break;
	default:
		assert( false );
		break;
	}

	int rows = getSectionRows(eSection);
	int cols = getSectionColumns(eSection);

	pSize->x = sectionSize.x/cols;
	pSize->y = sectionSize.y/rows;

	int itemCol = iItemIndex % cols;
	int itemRow = iItemIndex/cols;

	pPosition->x = itemCol * pSize->x;
	pPosition->y = itemRow * pSize->y;
}

void UIScene_InventoryMenu::setSectionSelectedSlot(ESceneSection eSection, int x, int y)
{
	int cols = getSectionColumns(eSection);

	int index = (y * cols) + x;

	UIControl_SlotList *slotList = nullptr;
	switch( eSection )
	{
	case eSectionInventoryArmor:
		slotList = &m_slotListArmor;
		break;
	case eSectionInventoryInventory:
		slotList = &m_slotListInventory;
		break;
	case eSectionInventoryUsing:
		slotList = &m_slotListHotbar;
		break;
	}

	slotList->setHighlightSlot(index);
}

UIControl *UIScene_InventoryMenu::getSection(ESceneSection eSection)
{
	UIControl *control = nullptr;
	switch( eSection )
	{
	case eSectionInventoryArmor:
		control = &m_slotListArmor;
		break;
	case eSectionInventoryInventory:
		control = &m_slotListInventory;
		break;
	case eSectionInventoryUsing:
		control = &m_slotListHotbar;
		break;
	}
	return control;
}

void UIScene_InventoryMenu::customDraw(IggyCustomDrawCallbackRegion *region)
{
	Minecraft *pMinecraft = Minecraft::GetInstance();
    if(pMinecraft->localplayers[m_iPad] == nullptr || pMinecraft->localgameModes[m_iPad] == nullptr)
		return;

	if(wcscmp((wchar_t *)region->name, L"player") == 0)
	{
		CustomDrawData *d = ui.calculateCustomDraw(region);
		if(d)
		{
			m_playerRegionX0 = d->x0;
			delete d;
		}

		CustomDrawData *customDrawRegion = ui.setupCustomDraw(this, region);
		delete customDrawRegion;

		m_playerPreview.render(region);

		ui.endCustomDraw(region);
		return;
	}

	int slotId = -1;
	if(swscanf(static_cast<wchar_t *>(region->name), L"slot_%d", &slotId) == 1)
	{
     // Capture one slot region each frame for transform matrix setup.
		if(!m_bootsSlotData.valid || slotId == InventoryMenu::ARMOR_SLOT_END - 1)
		{
			CustomDrawData *d = ui.calculateCustomDraw(region);
			if(d)
			{
				m_bootsSlotData.x0 = d->x0;
				m_bootsSlotData.y0 = d->y0;
				m_bootsSlotData.x1 = d->x1;
				m_bootsSlotData.y1 = d->y1;
				memcpy(m_bootsSlotData.mat, d->mat, sizeof(d->mat));
				m_bootsSlotData.valid = true;
				delete d;
			}
		}

		// Important:
		// Do NOT call UIScene_AbstractContainerMenu::customDraw(region) for slots.
		// We draw all modern slot contents ourselves after the SWF render.
		return;
	}

	// Let non-slot custom draw regions continue through the normal path.
	UIScene_AbstractContainerMenu::customDraw(region);
}

void UIScene_InventoryMenu::render(S32 width, S32 height, C4JRender::eViewportType viewport)
{
  // Capture fresh SWF slot data every frame.
	m_bootsSlotData.valid = false;

  // Draw SWF first (input/callback shell), then override visuals with
	// modern inventory background + C++ slot rendering.
	UIScene_AbstractContainerMenu::render(width, height, viewport);

	if(!m_bootsSlotData.valid || m_menu == nullptr)
		return;

	Minecraft *pMinecraft = Minecraft::GetInstance();
	if(!pMinecraft || !pMinecraft->localplayers[m_iPad])
		return;

    constexpr float MODERN_INV_W = 176.0f;
	constexpr float MODERN_INV_H = 166.0f;
	constexpr float SLOT_ITEM_W = 16.0f;
	constexpr float SLOT_ITEM_H = 16.0f;

	const float leftPos = (static_cast<float>(m_movieWidth) - MODERN_INV_W) * 0.5f;
	const float topPos = (static_cast<float>(m_movieHeight) - MODERN_INV_H) * 0.5f;

	CustomDrawData bgData;
	bgData.x0 = leftPos;
	bgData.y0 = topPos;
    bgData.x1 = leftPos + MODERN_INV_W;
	bgData.y1 = topPos + MODERN_INV_H;
	memcpy(bgData.mat, m_bootsSlotData.mat, sizeof(bgData.mat));

	shared_ptr<MultiplayerLocalPlayer> oldPlayer = pMinecraft->player;
	if(m_iPad >= 0 && m_iPad < XUSER_MAX_COUNT)
		pMinecraft->player = pMinecraft->localplayers[m_iPad];

	ui.setupCustomDrawGameState();

	// Draw modern inventory background.
	ui.setupCustomDrawMatrices(this, &bgData);
  fillSolidRect(bgData.x0, bgData.y0, bgData.x1, bgData.y1, 0xc6c6c6);
	pMinecraft->textures->bindTexture(L"gui/container/inventory.png");
	RenderManager.StateSetBlendEnable(true);
	RenderManager.StateSetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	Tesselator *t = Tesselator::getInstance();
	t->begin();
	t->vertexUV(bgData.x0, bgData.y1, 0.0f, 0.0f, 1.0f);
	t->vertexUV(bgData.x1, bgData.y1, 0.0f, 1.0f, 1.0f);
	t->vertexUV(bgData.x1, bgData.y0, 0.0f, 1.0f, 0.0f);
	t->vertexUV(bgData.x0, bgData.y0, 0.0f, 0.0f, 0.0f);
	t->end();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	// Draw all slot contents according to menu slot coordinates.
	const unsigned int slotCount = m_menu->getSize();
	for(unsigned int i = 0; i < slotCount; ++i)
	{
		Slot *slot = m_menu->getSlot(i);
		if(!slot)
			continue;

		shared_ptr<ItemInstance> item = slot->getItem();
		if(!item)
			continue;

		CustomDrawData slotData;
     slotData.x0 = leftPos + static_cast<float>(slot->x);
		slotData.y0 = topPos + static_cast<float>(slot->y);
		slotData.x1 = slotData.x0 + SLOT_ITEM_W;
		slotData.y1 = slotData.y0 + SLOT_ITEM_H;
		memcpy(slotData.mat, m_bootsSlotData.mat, sizeof(slotData.mat));

		ui.setupCustomDrawMatrices(this, &slotData);
		_customDrawSlotControl(&slotData, m_iPad, item, 1.0f, item->isFoil(), true, false);
	}

	ui.endCustomDrawGameState();
	pMinecraft->player = oldPlayer;
}

void UIScene_InventoryMenu::handleTimerComplete(int id)
{
	if(id == INVENTORY_UPDATE_EFFECTS_TIMER_ID)
	{
		updateEffectsDisplay();
	}
}

void UIScene_InventoryMenu::updateEffectsDisplay()
{
	// Update with the current effects
	Minecraft *pMinecraft = Minecraft::GetInstance();
	shared_ptr<MultiplayerLocalPlayer> player = pMinecraft->localplayers[m_iPad];

	if(player == nullptr) return;

	vector<MobEffectInstance *> *activeEffects = player->getActiveEffects();

	// 4J - TomK setup time update value array size to update the active effects
	int iValue = 0;
	IggyDataValue *UpdateValue = new IggyDataValue[activeEffects->size()*2];

	for(auto& effect : *activeEffects)
	{
		if(effect->getDuration() >= m_bEffectTime[effect->getId()])
		{
			wstring effectString = app.GetString( effect->getDescriptionId() );//I18n.get(effect.getDescriptionId()).trim();
			if (effect->getAmplifier() > 0)
			{
				wstring potencyString = L"";
				switch(effect->getAmplifier())
				{
				case 1:
					potencyString = L" ";
					potencyString += app.GetString( IDS_POTION_POTENCY_1 );
					break;
				case 2:
					potencyString = L" ";
					potencyString += app.GetString( IDS_POTION_POTENCY_2 );
					break;
				case 3:
					potencyString = L" ";
					potencyString += app.GetString( IDS_POTION_POTENCY_3 );
					break;
				default:
					potencyString = app.GetString( IDS_POTION_POTENCY_0 );
					break;
				}
				effectString += potencyString;
			}
			int icon = 0;
			MobEffect *mobEffect = MobEffect::effects[effect->getId()];
			if (mobEffect->hasIcon())
			{
				icon = mobEffect->getIcon();
            }
			IggyDataValue result;
			IggyDataValue value[3];
			value[0].type = IGGY_DATATYPE_number;
			value[0].number = icon;

			IggyStringUTF16 stringVal;
			stringVal.string = (IggyUTF16*)effectString.c_str();
			stringVal.length = effectString.length();
			value[1].type = IGGY_DATATYPE_string_UTF16;
			value[1].string16 = stringVal;

			int seconds = effect->getDuration() / SharedConstants::TICKS_PER_SECOND;
			value[2].type = IGGY_DATATYPE_number;
			value[2].number = seconds;
			IggyResult out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcAddEffect , 3 , value );
		}

		if(MobEffect::effects[effect->getId()]->hasIcon())
		{
			// 4J - TomK set ids and remaining duration so we can update the timers accurately in one call! (this prevents performance related timer sync issues, especially on PSVita)
			UpdateValue[iValue].type = IGGY_DATATYPE_number;
			UpdateValue[iValue].number = MobEffect::effects[effect->getId()]->getIcon();
			UpdateValue[iValue + 1].type = IGGY_DATATYPE_number;
			UpdateValue[iValue + 1].number = (int)(effect->getDuration() / SharedConstants::TICKS_PER_SECOND);
			iValue+=2;
		}

		m_bEffectTime[effect->getId()] = effect->getDuration();
	}

	IggyDataValue result;
	IggyResult out = IggyPlayerCallMethodRS ( getMovie() , &result, IggyPlayerRootPath( getMovie() ), m_funcUpdateEffects , activeEffects->size()*2 , UpdateValue );

	delete activeEffects;
}
