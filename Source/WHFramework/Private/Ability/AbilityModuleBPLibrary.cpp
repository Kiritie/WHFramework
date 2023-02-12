// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleBPLibrary.h"

#include "Ability/AbilityModule.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Global/GlobalBPLibrary.h"

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

FPrimaryAssetType UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType InItemType)
{
	return *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("/Script/WHFramework.EAbilityItemType"), (int32)InItemType);
}

EAbilityItemType UAbilityModuleBPLibrary::AssetTypeToItemType(FPrimaryAssetType InAssetType)
{
	return (EAbilityItemType)UGlobalBPLibrary::GetEnumIndexByValueName(TEXT("/Script/WHFramework.EAbilityItemType"), InAssetType.ToString());
}

void UAbilityModuleBPLibrary::AddCustomInteractAction(int32 InInteractAction, const FString& InTypeName)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		AbilityModule->AddCustomInteractAction(InInteractAction, InTypeName);
	}
}

void UAbilityModuleBPLibrary::RemoveCustomInteractAction(int32 InInteractAction)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		AbilityModule->RemoveCustomInteractAction(InInteractAction);
	}
}

FText UAbilityModuleBPLibrary::GetInteractActionDisplayName(int32 InInteractAction)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->GetInteractActionDisplayName(InInteractAction);
	}
	return FText::GetEmpty();
}

AAbilityPickUpBase* UAbilityModuleBPLibrary::SpawnPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnPickUp(InItem, InLocation, InContainer);
	}
	return nullptr;
}

AAbilityPickUpBase* UAbilityModuleBPLibrary::SpawnPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnPickUp(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityCharacterBase* UAbilityModuleBPLibrary::SpawnCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnCharacter(InSaveData, InContainer);
	}
	return nullptr;
}

AAbilityVitalityBase* UAbilityModuleBPLibrary::SpawnVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	if(AAbilityModule* AbilityModule = AAbilityModule::Get())
	{
		return AbilityModule->SpawnVitality(InSaveData, InContainer);
	}
	return nullptr;
}
