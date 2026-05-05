#include "stdafx.h"
#include "net.minecraft.network.packet.h"
#include "net.minecraft.world.level.h"
#include "net.minecraft.commands.h"
#include "..\Minecraft.Client\MinecraftServer.h"
#include "CommandBlockEntity.h"
#include "ContentHooks.h"

CommandBlockEntity::CommandBlockEntity()
{
	successCount = 0;
	command = L"";
	name = L"@";
}

void CommandBlockEntity::setCommand(const wstring &command)
{
	this->command = command;
	setChanged();
}

wstring CommandBlockEntity::getCommand()
{
	return command;
}

int CommandBlockEntity::performCommand(Level *level)
{
   if (!level || level->isClientSide)
		return 0;

	if (g_executeModCommandText)
	{
		bool ok = g_executeModCommandText(static_cast<void*>(this), command.c_str());
		return ok ? 1 : 0;
	}

	return 0;
}

wstring CommandBlockEntity::getName()
{
	return name;
}

void CommandBlockEntity::setName(const wstring &name)
{
	this->name = name;
}

void CommandBlockEntity::sendMessage(const wstring& message, ChatPacket::EChatPacketMessage type, int customData , const wstring& additionalMessage)
{
}

bool CommandBlockEntity::hasPermission(EGameCommand command)
{
	return false;
}

void CommandBlockEntity::save(CompoundTag *tag)
{
	TileEntity::save(tag);
	tag->putString(L"Command", command);
	tag->putInt(L"SuccessCount", successCount);
	tag->putString(L"CustomName", name);
}

void CommandBlockEntity::load(CompoundTag *tag)
{
	TileEntity::load(tag);
	command = tag->getString(L"Command");
	successCount = tag->getInt(L"SuccessCount");
	if (tag->contains(L"CustomName")) name = tag->getString(L"CustomName");
}

Pos *CommandBlockEntity::getCommandSenderWorldPosition()
{
	return new Pos(x, y, z);
}

Level *CommandBlockEntity::getCommandSenderWorld()
{
	return getLevel();
}

shared_ptr<Packet> CommandBlockEntity::getUpdatePacket()
{
	CompoundTag *tag = new CompoundTag();
	save(tag);
	return std::make_shared<TileEntityDataPacket>(x, y, z, TileEntityDataPacket::TYPE_ADV_COMMAND, tag);
}

int CommandBlockEntity::getSuccessCount()
{
	return successCount;
}

void CommandBlockEntity::setSuccessCount(int successCount)
{
	this->successCount = successCount;
}

// 4J Added
shared_ptr<TileEntity> CommandBlockEntity::clone()
{
	shared_ptr<CommandBlockEntity> result = std::make_shared<CommandBlockEntity>();
	TileEntity::clone(result);

	result->successCount = successCount;
	result->command = command;
	result->name = name;

	return result;
}