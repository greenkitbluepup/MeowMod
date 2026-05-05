#include "stdafx.h"
#include "net.minecraft.network.packet.h"
#include "net.minecraft.world.level.h"
#include "net.minecraft.world.level.tile.h"
#include "net.minecraft.world.level.tile.entity.h"
#include "TileEntity.h"
#include "PistonPieceEntity.h"
#include "ContentHooks.h"



TileEntity::idToCreateMapType TileEntity::idCreateMap = unordered_map<wstring, tileEntityCreateFn>();
TileEntity::classToIdMapType TileEntity::classIdMap = unordered_map<eINSTANCEOF, wstring, eINSTANCEOFKeyHash, eINSTANCEOFKeyEq>();

void TileEntity::staticCtor()
{
	TileEntity::setId(FurnaceTileEntity::create, eTYPE_FURNACETILEENTITY, L"Furnace");
	TileEntity::setId(ChestTileEntity::create, eTYPE_CHESTTILEENTITY, L"Chest");
	TileEntity::setId(EnderChestTileEntity::create, eTYPE_ENDERCHESTTILEENTITY, L"EnderChest");
	TileEntity::setId(JukeboxTile::Entity::create, eTYPE_RECORDPLAYERTILE, L"RecordPlayer");
	TileEntity::setId(DispenserTileEntity::create, eTYPE_DISPENSERTILEENTITY, L"Trap");
	TileEntity::setId(DropperTileEntity::create, eTYPE_DROPPERTILEENTITY, L"Dropper");
	TileEntity::setId(SignTileEntity::create, eTYPE_SIGNTILEENTITY, L"Sign");
	TileEntity::setId(MobSpawnerTileEntity::create, eTYPE_MOBSPAWNERTILEENTITY, L"MobSpawner");
	TileEntity::setId(MusicTileEntity::create, eTYPE_MUSICTILEENTITY, L"Music");
	TileEntity::setId(PistonPieceEntity::create, eTYPE_PISTONPIECEENTITY, L"Piston");
	TileEntity::setId(BrewingStandTileEntity::create, eTYPE_BREWINGSTANDTILEENTITY, L"Cauldron");
	TileEntity::setId(EnchantmentTableEntity::create, eTYPE_ENCHANTMENTTABLEENTITY, L"EnchantTable");
	TileEntity::setId(TheEndPortalTileEntity::create, eTYPE_THEENDPORTALTILEENTITY, L"Airportal");
	TileEntity::setId(CommandBlockEntity::create, eTYPE_COMMANDBLOCKTILEENTITY, L"Control");
	TileEntity::setId(BeaconTileEntity::create, eTYPE_BEACONTILEENTITY, L"Beacon");
	TileEntity::setId(SkullTileEntity::create,eTYPE_SKULLTILEENTITY, L"Skull");
	TileEntity::setId(DaylightDetectorTileEntity::create, eTYPE_DAYLIGHTDETECTORTILEENTITY, L"DLDetector");
	TileEntity::setId(HopperTileEntity::create, eTYPE_HOPPERTILEENTITY, L"Hopper");
	TileEntity::setId(ComparatorTileEntity::create, eTYPE_COMPARATORTILEENTITY, L"Comparator");

	if (g_registerModTileEntities)
		g_registerModTileEntities();
}

void TileEntity::registerExternalFactory(const wstring& id, tileEntityCreateFn createFn)
{
	if (id.empty() || !createFn)
		return;
	if (idCreateMap.find(id) == idCreateMap.end())
		idCreateMap.insert(idToCreateMapType::value_type(id, createFn));
}

void TileEntity::setId(tileEntityCreateFn createFn, eINSTANCEOF clas, wstring id)
{
	// 4J Stu - Java has classIdMap.containsKey(id) which would never work as id is not of the type of the key in classIdMap
	// I have changed to use idClassMap instead so that we can still search from the string key
	// TODO 4J Stu - Exceptions
	if (idCreateMap.find(id) != idCreateMap.end() ) {}//throw new IllegalArgumentException("Duplicate id: " + id);
	idCreateMap.insert( idToCreateMapType::value_type(id, createFn) );
	classIdMap.insert( classToIdMapType::value_type( clas, id ) );
}

TileEntity::TileEntity()
{
	level = nullptr;
	x = y = z = 0;
	remove = false;
	data = -1;
	tile = nullptr;
	renderRemoveStage = e_RenderRemoveStageKeep;
   externalId = L"";
}

Level *TileEntity::getLevel()
{
	return level;
}

void TileEntity::setLevel(Level *level)
{
	this->level = level;
}

bool TileEntity::hasLevel()
{
	return level != nullptr;
}

void TileEntity::load(CompoundTag *tag)
{
	x = tag->getInt(L"x");
	y = tag->getInt(L"y");
	z = tag->getInt(L"z");
}

void TileEntity::save(CompoundTag *tag)
{
 wstring saveId;
	auto it = classIdMap.find(this->GetType());
	if (it != classIdMap.end())
		saveId = ((*it).second);
	else if (!externalId.empty())
		saveId = externalId;
	else
		return;

	tag->putString(L"id", saveId);
	tag->putInt(L"x", x);
	tag->putInt(L"y", y);
	tag->putInt(L"z", z);

	if (g_modTileEntityOnSave)
		g_modTileEntityOnSave(saveId.c_str(), static_cast<void*>(this), static_cast<void*>(tag));
}

void TileEntity::tick()
{
}

shared_ptr<TileEntity> TileEntity::loadStatic(CompoundTag *tag)
{
	shared_ptr<TileEntity> entity = nullptr;
    wstring loadId = tag->getString(L"id");
	auto it = idCreateMap.find(loadId);
    if (it != idCreateMap.end())
        entity = shared_ptr<TileEntity>(it->second());
	if (entity != nullptr)
	{
      entity->externalId = loadId;
		entity->load(tag);
       if (g_modTileEntityOnLoad)
			g_modTileEntityOnLoad(loadId.c_str(), static_cast<void*>(entity.get()), static_cast<void*>(tag));
	}
	else
	{
#ifdef _DEBUG
		app.DebugPrintf("Skipping TileEntity with id %ls.\n" , tag->getString(L"id").c_str() );
#endif
	}

	return entity;
}

int TileEntity::getData()
{
	if (data == -1) data = level->getData(x, y, z);
	return data;
}

void TileEntity::setData(int data, int updateFlags)
{
	this->data = data;
	level->setData(x, y, z, data, updateFlags);
}

void TileEntity::setChanged()
{
	if (level != nullptr)
	{
		data = level->getData(x, y, z);
		level->tileEntityChanged(x, y, z, shared_from_this());
		if (getTile() != nullptr) level->updateNeighbourForOutputSignal(x, y, z, getTile()->id);
	}
}

double TileEntity::distanceToSqr(double xPlayer, double yPlayer, double zPlayer)
{
	double xd = (x + 0.5) - xPlayer;
	double yd = (y + 0.5) - yPlayer;
	double zd = (z + 0.5) - zPlayer;
	return xd * xd + yd * yd + zd * zd;
}

double TileEntity::getViewDistance()
{
	return 64 * 64;
}

Tile *TileEntity::getTile()
{
	if( tile == nullptr ) tile = Tile::tiles[level->getTile(x, y, z)];
	return tile;
}

shared_ptr<Packet> TileEntity::getUpdatePacket()
{
	return nullptr;
}

bool TileEntity::isRemoved()
{
	return remove;
}

void TileEntity::setRemoved()
{
	remove = true;
}

void TileEntity::clearRemoved()
{
	remove = false;
}

bool TileEntity::triggerEvent(int b0, int b1)
{
	return false;
}

void TileEntity::clearCache()
{
	tile = nullptr;
	data = -1;
}

void TileEntity::setRenderRemoveStage( unsigned char stage )
{
	renderRemoveStage = stage;
}

bool TileEntity::shouldRemoveForRender()
{
	return (renderRemoveStage == e_RenderRemoveStageRemove);
}

void TileEntity::upgradeRenderRemoveStage()
{
	if( renderRemoveStage == e_RenderRemoveStageFlaggedAtChunk )
	{
		renderRemoveStage = e_RenderRemoveStageRemove;
	}
}

// 4J Added
void TileEntity::clone(shared_ptr<TileEntity> tileEntity)
{
	tileEntity->level = this->level;
	tileEntity->x = this->x;
	tileEntity->y = this->y;
	tileEntity->z = this->z;
	tileEntity->data = this->data;
	tileEntity->tile = this->tile;
}