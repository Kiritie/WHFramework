// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleStatics.h"

#include "Ability/AbilityModule.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Effects/EffectBase.h"

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
				default: break;
			}
			EffectInfo.Attributes.Add(AttributeInfo);
		}
		Effect->DurationMagnitude.GetStaticMagnitudeIfPossible(InLevel, EffectInfo.Duration);
		EffectInfo.Period = Effect->Period.GetValueAtLevel(InLevel);
	}
	return EffectInfo;
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

AAbilityProjectileBase* UAbilityModuleStatics::SpawnAbilityProjectile(const TSubclassOf<AAbilityProjectileBase>& InClass, AActor* InOwnerActor, const FGameplayAbilitySpecHandle& InAbilityHandle)
{
	return UAbilityModule::Get().SpawnAbilityProjectile(InClass, InOwnerActor, InAbilityHandle);
}

AActor* UAbilityModuleStatics::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	return UAbilityModule::Get().SpawnAbilityActor(InSaveData, InContainer);
}
