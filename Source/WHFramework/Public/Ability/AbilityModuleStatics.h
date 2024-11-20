// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityModuleTypes.h"
#include "Asset/AssetModuleStatics.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/MathStatics.h"
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
class AAbilityPickUpBase;
class AAbilityPawnBase;
class AAbilityProjectileBase;
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
	static bool GetNoiseRaceDatas(FVector2D InLocation, int32 InOffset, TArray<T>& OutDatas)
	{
		TArray<T> RaceDatas;
		if(UAssetModuleStatics::ReadDataTable(RaceDatas))
		{
			for(int32 i = 0; i < RaceDatas.Num(); i++)
			{
				const auto& RaceData = RaceDatas[i];
				const float NoiseHeight = UMathStatics::GetNoiseHeight(InLocation, FVector(RaceData.NoiseScale.X, RaceData.NoiseScale.Y, 1.f), InOffset * (i + 1));
				if(FMath::Abs(NoiseHeight) >= RaceData.NoiseScale.Z)
				{
					OutDatas.Add(RaceData);
				}
			}
		}
		return OutDatas.Num() > 0;
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Item
	static AAbilityItemBase* SpawnAbilityItem(FAbilityItem InItem, FVector InLocation, FRotator InRotation, ISceneContainerInterface* InContainer = nullptr);

	static AAbilityItemBase* SpawnAbilityItem(FAbilityItem InItem, AActor* InOwnerActor = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// PickUp
	static AAbilityPickUpBase* SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer = nullptr);

	static AAbilityPickUpBase* SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Projectile
	static AAbilityProjectileBase* SpawnAbilityProjectile(const TSubclassOf<AAbilityProjectileBase>& InClass, AActor* InOwnerActor = nullptr, const FGameplayAbilitySpecHandle& InAbilityHandle = FGameplayAbilitySpecHandle());

	//////////////////////////////////////////////////////////////////////////
	// Actor
	static AActor* SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);
};
