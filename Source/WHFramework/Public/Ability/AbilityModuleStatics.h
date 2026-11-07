// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityInfoTypes.h"
#include "Ability/Inventory/InventoryItemTypes.h"
#include "Asset/AssetModuleStatics.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/MathHelper.h"
#include "AbilityModuleStatics.generated.h"

class UAbilityBase;
class UEffectBase;
class AAbilityItemBase;
class AAbilityActorBase;
class UAbilitySystemComponentBase;
class UGameplayAbility;
class ISceneContainerInterface;
class AAbilityVitalityBase;
class AAbilityCharacterBase;
class AAbilityPawnBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Ability
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayAbility")
	static const UAbilityBase* GetAbilityBySpec(const FGameplayAbilitySpec& InAbilitySpec, bool& bInstance);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayAbility")
	static FAbilityInfo GetAbilityInfoByClass(TSubclassOf<UAbilityBase> InAbilityClass, float InLevel = 1);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayAbility")
	static FEffectInfo GetEffectInfoByClass(TSubclassOf<UEffectBase> InEffectClass, float InLevel = 1);

	//////////////////////////////////////////////////////////////////////////
	// Race
	template<class T>
	static bool GetNoiseRaceDatas(const UObject* WorldContextObject, FVector2D InLocation, int32 InOffset, TArray<T>& OutDatas)
	{
		TArray<T> RaceDatas;
		if(UAssetModuleStatics::ReadDataTable(WorldContextObject, RaceDatas))
		{
			for(int32 i = 0; i < RaceDatas.Num(); i++)
			{
				const auto& RaceData = RaceDatas[i];
				const float NoiseValue = FMathHelper::GetNoise2D(FVector2D(InLocation.X * RaceData.NoiseScale.X, InLocation.Y * RaceData.NoiseScale.Y), InOffset * (i + 1));
				if(FMath::Abs(NoiseValue) >= RaceData.NoiseScale.Z)
				{
					OutDatas.Add(RaceData);
				}
			}
		}
		return OutDatas.Num() > 0;
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Attribute
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category = "Ability|GameplayAbility")
	static FLinearColor GetAttributeColor(UWorld* InWorld, const FGameplayAttribute& InAttribute);

	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld"), Category = "Ability|GameplayAbility")
	static void SetAttributeColor(UWorld* InWorld, const FGameplayAttribute& InAttribute, const FLinearColor& InColor);

	//////////////////////////////////////////////////////////////////////////
	// Item
	static AAbilityItemBase* SpawnAbilityItem(UWorld* InWorld, FAbilityItem InItem, FVector InLocation, FRotator InRotation, ISceneContainerInterface* InContainer = nullptr);

	static AAbilityItemBase* SpawnAbilityItem(UWorld* InWorld, FAbilityItem InItem, AActor* InOwnerActor);

	//////////////////////////////////////////////////////////////////////////
	// Actor
	static AActor* SpawnAbilityActor(UWorld* InWorld, const FParameter& InSaveData, ISceneContainerInterface* InContainer = nullptr);
};
