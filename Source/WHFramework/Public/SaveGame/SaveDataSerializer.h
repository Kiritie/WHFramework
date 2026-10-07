#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Parameter/ParameterTypes.h"
#include "Templates/Function.h"

struct FModuleSaveFileHeader;

class WHFRAMEWORK_API FSaveDataSerializer
{
public:
	static bool SerializeParameter(const FParameter& InData, TArray<uint8>& OutBytes);
	static bool SerializeParameterBounded(const FParameter& InData, int32 InMaximumBytes,
		TArray<uint8>& OutBytes, TFunctionRef<bool(TArray<uint8>&, int32)> InReserve);

	static bool DeserializeParameter(const TArray<uint8>& InBytes, FParameter& OutData);
	static bool DeserializeParameterView(TConstArrayView<uint8> InBytes, FParameter& OutData);

	static bool BuildModuleFile(FName ModuleName, int32 ModuleVersion, const FParameter& Data, TArray<uint8>& OutFileBytes);
	static bool WriteModuleHeaderBounded(FName InModuleName, int32 InModuleVersion,
		int64 InPayloadSize, uint32 InPayloadCrc, TArray<uint8>& OutFileBytes,
		TFunctionRef<bool(TArray<uint8>&, int32)> InReserve);

	static bool ReadModuleFile(const TArray<uint8>& InFileBytes, FModuleSaveFileHeader& OutHeader, FParameter& OutData);

	static bool MarkSaveDataSaved(FParameter& InOutData);
};
