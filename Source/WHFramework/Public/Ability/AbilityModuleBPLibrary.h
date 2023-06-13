// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityModuleTypes.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/MathBPLibrary.h"
#include "AbilityModuleBPLibrary.generated.h"

class UAbilitySystemComponentBase;
class UGameplayAbility;
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
	static bool GetAbilityInfoByClass(TSubclassOf<UGameplayAbility> AbilityClass, FAbilityInfo& OutAbilityInfo);

	//////////////////////////////////////////////////////////////////////////
	// Item
	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static FPrimaryAssetType ItemTypeToAssetType(EAbilityItemType InItemType);

	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static EAbilityItemType AssetTypeToItemType(FPrimaryAssetType InAssetType);

	//////////////////////////////////////////////////////////////////////////
	// Interact
	UFUNCTION(BlueprintCallable, Category = "AbilityModuleBPLibrary")
	static void AddCustomInteractAction(int32 InInteractAction, const FString& InTypeName);

	UFUNCTION(BlueprintCallable, Category = "AbilityModuleBPLibrary")
	static void RemoveCustomInteractAction(int32 InInteractAction);

	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static FText GetInteractActionDisplayName(int32 InInteractAction);

	//////////////////////////////////////////////////////////////////////////
	// Race
	template<class T>
	static bool GetNoiseRaceDatas(FVector2D InLocation, int32 InOffset, TArray<T>& OutDatas)
	{
		TArray<T> raceDatas;
		if(UAssetModuleBPLibrary::ReadDataTable(raceDatas))
		{
			for(int32 i = 0; i < raceDatas.Num(); i++)
			{
				const auto& raceData = raceDatas[i];
				const float noiseHeight = UMathBPLibrary::GetNoiseHeight(InLocation, FVector(raceData.NoiseScale.X, raceData.NoiseScale.Y, 1.f), InOffset * (i + 1), true);
				if(noiseHeight >= raceData.NoiseScale.Z)
				{
					OutDatas.Add(raceData);
				}
			}
		}
		return OutDatas.Num() > 0;
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
