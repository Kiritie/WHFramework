// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityModuleTypes.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/MathBPLibrary.h"
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
	static TArray<T> GetNoiseRaceDatas(FIndex InIndex, int32 InOffset)
	{
		TArray<T> raceDatas1;
		TArray<T> raceDatas2;
		if(UAssetModuleBPLibrary::ReadDataTable(raceDatas1))
		{
			for(auto raceData = raceDatas1.CreateConstIterator(); raceData; ++raceData)
			{
				const float noiseHeight = UMathBPLibrary::GetNoiseHeight(InIndex, raceData->NoiseScale, InOffset);
				WHDebug(FString::Printf(TEXT("%s : %f"), *InIndex.ToString(), noiseHeight));
				if(noiseHeight > 0.f)
				{
					//raceDatas2.Add(iter);
				}
			}
		}
		return raceDatas2;
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
