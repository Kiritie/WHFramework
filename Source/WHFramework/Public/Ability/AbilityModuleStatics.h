// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityModuleTypes.h"
#include "Asset/AssetModuleStatics.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/MathStatics.h"
#include "AbilityModuleStatics.generated.h"

class AAbilityItemBase;
class AAbilityActorBase;
class UAbilitySystemComponentBase;
class UGameplayAbility;
class ISceneContainerInterface;
class AAbilityVitalityBase;
class AAbilityCharacterBase;
class AAbilityPickUpBase;
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
	static const UGameplayAbility* GetGameplayAbilityBySpec(const FGameplayAbilitySpec& AbilitySpec, bool& bIsInstance);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayAbility")
	static bool GetAbilityInfoByClass(TSubclassOf<UGameplayAbility> AbilityClass, FAbilityInfo& OutAbilityInfo);

	//////////////////////////////////////////////////////////////////////////
	// Race
	template<class T>
	static bool GetNoiseRaceDatas(FVector2D InLocation, int32 InOffset, TArray<T>& OutDatas)
	{
		TArray<T> raceDatas;
		if(UAssetModuleStatics::ReadDataTable(raceDatas))
		{
			for(int32 i = 0; i < raceDatas.Num(); i++)
			{
				const auto& raceData = raceDatas[i];
				const float noiseHeight = UMathStatics::GetNoiseHeight(InLocation, FVector(raceData.NoiseScale.X, raceData.NoiseScale.Y, 1.f), InOffset * (i + 1));
				if(noiseHeight >= raceData.NoiseScale.Z)
				{
					OutDatas.Add(raceData);
				}
			}
		}
		return OutDatas.Num() > 0;
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Item
	static AAbilityItemBase* SpawnAbilityItem(FAbilityItem InItem, FVector InLocation = FVector::ZeroVector, FRotator InRotation = FRotator::ZeroRotator, ISceneContainerInterface* InContainer = nullptr);

	static AAbilityItemBase* SpawnAbilityItem(FAbilityItem InItem, AActor* InOwnerActor);

	//////////////////////////////////////////////////////////////////////////
	// PickUp
	static AAbilityPickUpBase* SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation = FVector::ZeroVector, ISceneContainerInterface* InContainer = nullptr);

	static AAbilityPickUpBase* SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Actor
	static AAbilityActorBase* SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Vitality
	static AAbilityVitalityBase* SpawnAbilityVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Pawn
	static AAbilityPawnBase* SpawnAbilityPawn(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Character
	static AAbilityCharacterBase* SpawnAbilityCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);
};
