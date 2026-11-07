// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Ability/AbilityAttributeAccessors.h"
#include "Ability/AbilityInfoTypes.h"
#include "Ability/Inventory/InventoryItemTypes.h"

#include "GameplayModMagnitudeCalculation.h"
#include "Ability/AbilityModule.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Effects/EffectBase.h"
#include "Ability/Effects/Calculation/UEffectCalculation_Level.h"

const UAbilityBase* UAbilityModuleStatics::GetAbilityBySpec(const FGameplayAbilitySpec& InAbilitySpec, bool& bInstance)
{
	UAbilityBase* AbilityInstance = Cast<UAbilityBase>(InAbilitySpec.GetPrimaryInstance());
	bInstance = true;

	if (!AbilityInstance)
	{
		AbilityInstance = Cast<UAbilityBase>(InAbilitySpec.Ability);
		bInstance = false;
	}
	return AbilityInstance;
}

FAbilityInfo UAbilityModuleStatics::GetAbilityInfoByClass(TSubclassOf<UAbilityBase> InAbilityClass, float InLevel)
{
	FAbilityInfo AbilityInfo;
	if (UAbilityBase* Ability = InAbilityClass.GetDefaultObject())
	{
		for (auto& Iter : Ability->GetEffectClasses())
		{
			AbilityInfo.Effects.Add(GetEffectInfoByClass(Iter, InLevel));
		}
		if (Ability->GetCostGameplayEffect() && Ability->GetCostGameplayEffect()->Modifiers.Num() > 0)
		{
			const FGameplayModifierInfo ModifierInfo = Ability->GetCostGameplayEffect()->Modifiers[0];
			AbilityInfo.CostAttribute = ModifierInfo.Attribute;
			ModifierInfo.ModifierMagnitude.GetStaticMagnitudeIfPossible(InLevel, AbilityInfo.CostValue);
		}
		if (Ability->GetCooldownGameplayEffect())
		{
			Ability->GetCooldownGameplayEffect()->DurationMagnitude.GetStaticMagnitudeIfPossible(InLevel, AbilityInfo.CooldownDuration);
		}
	}
	return AbilityInfo;
}

FEffectInfo UAbilityModuleStatics::GetEffectInfoByClass(TSubclassOf<UEffectBase> InEffectClass, float InLevel)
{
	FEffectInfo EffectInfo;
	if(UEffectBase* Effect = InEffectClass.GetDefaultObject())
	{
		for(auto& Iter :  Effect->Modifiers)
		{
			FAttributeInfo AttributeInfo;
			AttributeInfo.Attribute = Iter.Attribute;
			AttributeInfo.ModifierOp = Iter.ModifierOp;
			switch(Iter.ModifierMagnitude.GetMagnitudeCalculationType())
			{
				case EGameplayEffectMagnitudeCalculation::ScalableFloat:
				{
					Iter.ModifierMagnitude.GetStaticMagnitudeIfPossible(InLevel, AttributeInfo.Value);
					break;
				}
				case EGameplayEffectMagnitudeCalculation::AttributeBased:
				{
					FAttributeBasedFloat BasedMagnitude = Iter.ModifierMagnitude.*GetAttributeBasedMagnitude();
					AttributeInfo.BaseAttribute = BasedMagnitude.BackingAttribute.AttributeToCapture;
					AttributeInfo.AttributeSource = BasedMagnitude.BackingAttribute.AttributeSource;
					AttributeInfo.Value = BasedMagnitude.Coefficient.GetValueAtLevel(InLevel);
					break;
				}
				case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
				{
					FCustomCalculationBasedFloat CustomCalculation = Iter.ModifierMagnitude.*GetCustomMagnitude();
					AttributeInfo.Value = CustomCalculation.Coefficient.GetValueAtLevel(InLevel);
					if(CustomCalculation.CalculationClassMagnitude == UUEffectCalculation_Level::StaticClass())
					{
						AttributeInfo.Value *= InLevel;
					}
					break;
				}
				default: break;
			}
			EffectInfo.Attributes.Add(AttributeInfo);
		}
		Effect->DurationMagnitude.GetStaticMagnitudeIfPossible(InLevel, EffectInfo.Duration);
		EffectInfo.Period = Effect->Period.GetValueAtLevel(InLevel);
	}
	return EffectInfo;
}

FLinearColor UAbilityModuleStatics::GetAttributeColor(UWorld* InWorld, const FGameplayAttribute& InAttribute)
{
	const UAbilityModule* Module = UAbilityModule::GetPtr(InWorld);
	return Module ? Module->GetAttributeColor(InAttribute) : FLinearColor::White;
}

void UAbilityModuleStatics::SetAttributeColor(UWorld* InWorld, const FGameplayAttribute& InAttribute, const FLinearColor& InColor)
{
	if (UAbilityModule* Module = UAbilityModule::GetPtr(InWorld)) Module->SetAttributeColor(InAttribute, InColor);
}

AAbilityItemBase* UAbilityModuleStatics::SpawnAbilityItem(UWorld* InWorld, FAbilityItem InItem, FVector InLocation, FRotator InRotation, ISceneContainerInterface* InContainer)
{
	UAbilityModule* Module = UAbilityModule::GetPtr(InWorld);
	UObject* Container = InContainer ? InContainer->_getUObject() : nullptr;
	return Module && (!InContainer || (Container && Container->GetWorld() == InWorld))
		? Module->SpawnAbilityItem(InItem, InLocation, InRotation, InContainer) : nullptr;
}

AAbilityItemBase* UAbilityModuleStatics::SpawnAbilityItem(UWorld* InWorld, FAbilityItem InItem, AActor* InOwnerActor)
{
	UAbilityModule* Module = UAbilityModule::GetPtr(InWorld);
	return Module && (!InOwnerActor || InOwnerActor->GetWorld() == InWorld) ? Module->SpawnAbilityItem(InItem, InOwnerActor) : nullptr;
}


AActor* UAbilityModuleStatics::SpawnAbilityActor(UWorld* InWorld, const FParameter& InSaveData, ISceneContainerInterface* InContainer)
{
	UAbilityModule* Module = UAbilityModule::GetPtr(InWorld);
	UObject* Container = InContainer ? InContainer->_getUObject() : nullptr;
	return Module && (!InContainer || (Container && Container->GetWorld() == InWorld))
		? Module->SpawnAbilityActor(InSaveData, InContainer) : nullptr;
}
