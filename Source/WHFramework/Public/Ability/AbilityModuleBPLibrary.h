// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityModuleTypes.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AbilityModuleBPLibrary.generated.h"

class UAbilitySystemComponentBase;
class UAbilityBase;
class ISceneContainerInterface;
class AAbilityVitalityBase;
class AAbilityCharacterBase;
class AAbilityPickUpBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Ability
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayAbility")
	static const UGameplayAbility* GetGameplayAbilityBySpec(const FGameplayAbilitySpec& AbilitySpec, bool& bIsInstance);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayAbility")
	static bool GetAbilityInfoByClass(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo);

	//////////////////////////////////////////////////////////////////////////
	// Item
	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static FPrimaryAssetType ItemTypeToAssetType(EAbilityItemType InItemType);

	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static EAbilityItemType AssetTypeToItemType(FPrimaryAssetType InAssetType);

	//////////////////////////////////////////////////////////////////////////
	// Interact
	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static FText GetInteractActionDisplayName(int32 InInteractAction);

	//////////////////////////////////////////////////////////////////////////
	// Race
	template<class T>
	static T GetRandomRaceData()
	{
		TArray<T> raceDatas;
		if(UAssetModuleBPLibrary::ReadDataTable(raceDatas))
		{
			float tmpNum1 = 0;
			float tmpNum2 = 0;
			for (int32 i = 0; i < raceDatas.Num(); i++)
			{
				tmpNum1 += raceDatas[i].Proportion;
			}
			tmpNum1 = FMath::FRandRange(0, tmpNum1);
			for (int32 i = 0; i < raceDatas.Num(); i++)
			{
				tmpNum2 += raceDatas[i].Proportion;
				if (tmpNum1 <= tmpNum2)
				{
					return raceDatas[i];
				}
			}
		}
		return T();
	}

	//////////////////////////////////////////////////////////////////////////
	// PickUp
	static AAbilityPickUpBase* SpawnPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer = nullptr);

	static AAbilityPickUpBase* SpawnPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Character
	static AAbilityCharacterBase* SpawnCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Vitality
	static AAbilityVitalityBase* SpawnVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);
};
