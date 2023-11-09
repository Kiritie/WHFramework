// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleStatics.h"

#include "Ability/AbilityModule.h"
#include "Ability/Abilities/AbilityBase.h"

const UGameplayAbility* UAbilityModuleStatics::GetGameplayAbilityBySpec(const FGameplayAbilitySpec& AbilitySpec, bool& bIsInstance)
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

bool UAbilityModuleStatics::GetAbilityInfoByClass(TSubclassOf<UGameplayAbility> AbilityClass, FAbilityInfo& OutAbilityInfo)
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

ECollisionChannel UAbilityModuleStatics::GetPickUpTraceChannel()
{
	if(UAbilityModule* AbilityModule = UAbilityModule::Get())
	{
		return AbilityModule->GetPickUpTraceChannel();
	}
	return ECC_MAX;
}

AAbilityPickUpBase* UAbilityModuleStatics::SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(UAbilityModule* AbilityModule = UAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityPickUp(InItem, InLocation, InContainer);
	}
	return nullptr;
}

AAbilityPickUpBase* UAbilityModuleStatics::SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(UAbilityModule* AbilityModule = UAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityPickUp(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityActorBase* UAbilityModuleStatics::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(UAbilityModule* AbilityModule = UAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityActor(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityCharacterBase* UAbilityModuleStatics::SpawnAbilityCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(UAbilityModule* AbilityModule = UAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityCharacter(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityPawnBase* UAbilityModuleStatics::SpawnAbilityPawn(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(UAbilityModule* AbilityModule = UAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityPawn(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityVitalityBase* UAbilityModuleStatics::SpawnAbilityVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(UAbilityModule* AbilityModule = UAbilityModule::Get())
	{
		return AbilityModule->SpawnAbilityVitality(InSaveData, InContainer);
	}
	return nullptr;
}
