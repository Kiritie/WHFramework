// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Parameter/ParameterTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "CharacterModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterSaveRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid CharacterId;

	UPROPERTY()
	FPrimaryAssetId CharacterAssetId;

	UPROPERTY()
	FParameter Data;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FGuid CurrentCharacterId;

	UPROPERTY()
	TArray<FCharacterSaveRecord> Characters;

	UPROPERTY()
	FParameter PlayerControllerData;

	virtual void MakeSaved() override
	{
		Super::MakeSaved();
		auto Mark = [](FParameter& Data)
		{
			const UScriptStruct* Struct = Data.GetStructType();
			if (Struct && Struct->IsChildOf(FSaveData::StaticStruct()))
			{
				reinterpret_cast<FSaveData*>(Data.GetMutableStructMemory())->MakeSaved();
			}
		};
		Mark(PlayerControllerData);
		for (FCharacterSaveRecord& Record : Characters)
		{
			Mark(Record.Data);
		}
	}
};
