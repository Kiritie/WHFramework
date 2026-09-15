#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterTypes.h"

struct FModuleSaveFileHeader;

class WHFRAMEWORK_API FSaveDataSerializer
{
public:
	static bool SerializeParameter(const FParameter& InData, TArray<uint8>& OutBytes);

	static bool DeserializeParameter(const TArray<uint8>& InBytes, FParameter& OutData);

	static bool BuildModuleFile(FName ModuleName, int32 ModuleVersion, const FParameter& Data, TArray<uint8>& OutFileBytes);

	static bool ReadModuleFile(const TArray<uint8>& InFileBytes, FModuleSaveFileHeader& OutHeader, FParameter& OutData);

	static bool MarkSaveDataSaved(FParameter& InOutData);
};
