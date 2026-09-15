#include "SaveGame/SaveDataSerializer.h"

#include "SaveGame/SaveGameModuleTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "WHFrameworkCoreTypes.h"

bool FSaveDataSerializer::SerializeParameter(const FParameter& InData, TArray<uint8>& OutBytes)
{
	if(!InData.HasValue())
	{
		return false;
	}

	OutBytes.Reset();
	FMemoryWriter Writer(OutBytes, true);
	FSaveDataArchive Archive(Writer);
	FParameter::StaticStruct()->SerializeItem(Archive, const_cast<FParameter*>(&InData), nullptr);
	return !Archive.IsError();
}

bool FSaveDataSerializer::DeserializeParameter(const TArray<uint8>& InBytes, FParameter& OutData)
{
	if(InBytes.IsEmpty())
	{
		return false;
	}

	FMemoryReader Reader(InBytes, true);
	FSaveDataArchive Archive(Reader);
	OutData.Reset();
	FParameter::StaticStruct()->SerializeItem(Archive, &OutData, nullptr);
	if(Archive.IsError() || !OutData.HasValue())
	{
		OutData.Reset();
		return false;
	}

	MarkSaveDataSaved(OutData);
	return true;
}

bool FSaveDataSerializer::MarkSaveDataSaved(FParameter& InOutData)
{
	const UScriptStruct* Struct = InOutData.GetStructType();
	uint8* Memory = InOutData.GetMutableStructMemory();
	if(!Struct || !Memory || !Struct->IsChildOf(FSaveData::StaticStruct()))
	{
		return false;
	}

	reinterpret_cast<FSaveData*>(Memory)->MakeSaved();
	return true;
}

bool FSaveDataSerializer::BuildModuleFile(FName ModuleName, int32 ModuleVersion, const FParameter& Data, TArray<uint8>& OutFileBytes)
{
	TArray<uint8> Payload;
	if(!SerializeParameter(Data, Payload))
	{
		return false;
	}

	FModuleSaveFileHeader Header;
	Header.ModuleName = ModuleName;
	Header.ModuleVersion = ModuleVersion;
	Header.PayloadSize = Payload.Num();
	Header.PayloadCrc = Payload.IsEmpty() ? 0 : FCrc::MemCrc32(Payload.GetData(), Payload.Num());

	OutFileBytes.Reset();
	FMemoryWriter Writer(OutFileBytes, true);
	Writer << Header;
	Writer.Serialize(Payload.GetData(), Payload.Num());
	return !Writer.IsError();
}

bool FSaveDataSerializer::ReadModuleFile(const TArray<uint8>& InFileBytes, FModuleSaveFileHeader& OutHeader, FParameter& OutData)
{
	if(InFileBytes.IsEmpty())
	{
		return false;
	}

	FMemoryReader Reader(InFileBytes, true);
	Reader << OutHeader;
	if(Reader.IsError() || OutHeader.Magic != FModuleSaveFileHeader::MagicValue || OutHeader.StorageVersion != 1)
	{
		return false;
	}

	const int64 Remaining = Reader.TotalSize() - Reader.Tell();
	if(OutHeader.PayloadSize <= 0 || OutHeader.PayloadSize > Remaining || OutHeader.PayloadSize > MAX_int32)
	{
		return false;
	}

	TArray<uint8> Payload;
	Payload.SetNumUninitialized(static_cast<int32>(OutHeader.PayloadSize));
	Reader.Serialize(Payload.GetData(), Payload.Num());
	if(Reader.IsError() || FCrc::MemCrc32(Payload.GetData(), Payload.Num()) != OutHeader.PayloadCrc)
	{
		return false;
	}

	return DeserializeParameter(Payload, OutData);
}
