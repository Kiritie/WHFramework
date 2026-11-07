#pragma once

#include "CoreMinimal.h"

struct FModuleSaveFileHeader
{
	static constexpr uint32 MagicValue = 0x44574D53;
	static constexpr int32 CurrentStorageVersion = 2;

	uint32 Magic = MagicValue;
	int32 StorageVersion = CurrentStorageVersion;
	FName ModuleName;
	int32 ModuleVersion = 1;
	int64 PayloadSize = 0;
	uint32 PayloadCrc = 0;

	friend FArchive& operator<<(FArchive& Ar, FModuleSaveFileHeader& Value)
	{
		Ar << Value.Magic;
		Ar << Value.StorageVersion;
		Ar << Value.ModuleName;
		Ar << Value.ModuleVersion;
		Ar << Value.PayloadSize;
		Ar << Value.PayloadCrc;
		return Ar;
	}
};
