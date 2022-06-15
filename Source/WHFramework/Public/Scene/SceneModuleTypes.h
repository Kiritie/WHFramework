// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Ability/AbilityModuleTypes.h"

#include "SceneModuleTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncLoadLevelFinished, FName, InLevelPath);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncUnloadLevelFinished, FName, InLevelPath);

/**
* 世界文本风格
*/
UENUM(BlueprintType)
enum class EWorldTextStyle : uint8
{
	// 普通
	Normal,
	// 强调
	Stress
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPickUpSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector Location;
	
	FORCEINLINE FPickUpSaveData()
	{
		Item = FAbilityItem::Empty;
		Location = FVector::ZeroVector;
	}
};
