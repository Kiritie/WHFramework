// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AbilityModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static FPrimaryAssetType GetAssetTypeByItemType(EAbilityItemType InItemType);

	//////////////////////////////////////////////////////////////////////////
	// Interact
	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static FText GetInteractActionDisplayName(int32 InInteractAction);

	//////////////////////////////////////////////////////////////////////////
	// Race
	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static FVitalityRaceData RandomVitalityRaceData();
	
	UFUNCTION(BlueprintPure, Category = "AbilityModuleBPLibrary")
	static FCharacterRaceData RandomCharacterRaceData();
};
