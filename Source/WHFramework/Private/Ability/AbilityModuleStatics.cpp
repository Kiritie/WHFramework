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

AAbilityItemBase* UAbilityModuleStatics::SpawnAbilityItem(FAbilityItem InItem, FVector InLocation, FRotator InRotation, ISceneContainerInterface* InContainer)
{
	return UAbilityModule::Get().SpawnAbilityItem(InItem, InLocation, InRotation, InContainer);
}

AAbilityItemBase* UAbilityModuleStatics::SpawnAbilityItem(FAbilityItem InItem, AActor* InOwnerActor)
{
	return UAbilityModule::Get().SpawnAbilityItem(InItem, InOwnerActor);
}

AAbilityPickUpBase* UAbilityModuleStatics::SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	return UAbilityModule::Get().SpawnAbilityPickUp(InItem, InLocation, InContainer);
}

AAbilityPickUpBase* UAbilityModuleStatics::SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	return UAbilityModule::Get().SpawnAbilityPickUp(InSaveData, InContainer);
}

AAbilityActorBase* UAbilityModuleStatics::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	return UAbilityModule::Get().SpawnAbilityActor(InSaveData, InContainer);
}

AAbilityCharacterBase* UAbilityModuleStatics::SpawnAbilityCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	return UAbilityModule::Get().SpawnAbilityCharacter(InSaveData, InContainer);
}

AAbilityPawnBase* UAbilityModuleStatics::SpawnAbilityPawn(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	return UAbilityModule::Get().SpawnAbilityPawn(InSaveData, InContainer);
}

AAbilityVitalityBase* UAbilityModuleStatics::SpawnAbilityVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	return UAbilityModule::Get().SpawnAbilityVitality(InSaveData, InContainer);
}
