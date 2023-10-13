// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleBPLibrary.h"

#include "Ability/AbilityModule.h"
#include "Ability/Abilities/AbilityBase.h"

const UGameplayAbility* UAbilityModuleBPLibrary::GetGameplayAbilityBySpec(const FGameplayAbilitySpec& AbilitySpec, bool& bIsInstance)
{
	UGameplayAbility* AbilityInstance = AbilitySpec.GetPrimaryInstance();
	bIsInstance = true;

	if (!AbilityInstance)
	{
		AbilityInstance = AbilitySpec.Ability;
		bIsInstance = false;
	}
	return AbilityInstance;
}

bool UAbilityModuleBPLibrary::GetAbilityInfoByClass(TSubclassOf<UGameplayAbility> AbilityClass, FAbilityInfo& OutAbilityInfo)
{
	if (AbilityClass != nullptr)
	{
		UGameplayAbility* Ability = AbilityClass.GetDefaultObject();
		if (Ability->GetCostGameplayEffect()->Modifiers.Num() > 0)
		{
			const FGameplayModifierInfo ModifierInfo = Ability->GetCostGameplayEffect()->Modifiers[0];
			OutAbilityInfo.CostAttribute = ModifierInfo.Attribute;
			ModifierInfo.ModifierMagnitude.GetStaticMagnitudeIfPossible(1, OutAbilityInfo.CostValue);
		}
		Ability->GetCooldownGameplayEffect()->DurationMagnitude.GetStaticMagnitudeIfPossible(1, OutAbilityInfo.CooldownDuration);
		return true;
	}
	return false;
}

ECollisionChannel UAbilityModuleBPLibrary::GetPickUpTraceChannel()
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->GetPickUpTraceChannel();
	}
	return ECC_MAX;
}

AAbilityPickUpBase* UAbilityModuleBPLibrary::SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityPickUp(InItem, InLocation, InContainer);
	}
	return nullptr;
}

AAbilityPickUpBase* UAbilityModuleBPLibrary::SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityPickUp(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityActorBase* UAbilityModuleBPLibrary::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityActor(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityCharacterBase* UAbilityModuleBPLibrary::SpawnAbilityCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityCharacter(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityPawnBase* UAbilityModuleBPLibrary::SpawnAbilityPawn(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityPawn(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityVitalityBase* UAbilityModuleBPLibrary::SpawnAbilityVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityVitality(InSaveData, InContainer);
	}
	return nullptr;
}
