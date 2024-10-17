// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGame/SaveGameModuleTypes.h"

#include "CharacterModuleTypes.generated.h"

class ACharacterBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FCharacterModuleSaveData()
	{
		CurrentCharacter = nullptr;
	}

public:
	UPROPERTY(BlueprintReadWrite)
	ACharacterBase* CurrentCharacter;
};
