// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Effects/EffectBase.h"
#include "Ability/Effects/Calculation/UEffectCalculation_Level.h"
#include "GameplayEffectComponent.h"

void UEffectBase::OnSpawn_Implementation(const FParameter& InParam)
{
}

void UEffectBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	const UEffectBase* Definition = GetClass()->GetDefaultObject<UEffectBase>();
	if (Definition != this)
	{
		Modifiers = Definition->Modifiers;
	}
}


bool UEffectBase::SupportsPersistence(float Level) const
{
	if (!FMath::IsFinite(Level) || !bPersistAcrossSave || DurationPolicy == EGameplayEffectDurationType::Instant || Executions.Num() != 0 || StackingType == EGameplayEffectStackingType::AggregateBySource)
	{
		return false;
	}
	if (Period.GetValueAtLevel(Level) > 0.f && bExecutePeriodicEffectOnApplication)
	{
		return false;
	}
	for (const FGameplayModifierInfo& Modifier : Modifiers)
	{
		if (Modifier.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::CustomCalculationClass)
		{
			return false;
		}
		TArray<FGameplayEffectAttributeCaptureDefinition> Captures;
		Modifier.ModifierMagnitude.GetAttributeCaptureDefinitions(Captures);
		for (const FGameplayEffectAttributeCaptureDefinition& Capture : Captures)
		{
			if (Capture.AttributeSource == EGameplayEffectAttributeCaptureSource::Source || Capture.bSnapshot)
			{
				return false;
			}
		}
	}
	return true;
}

bool UEffectBase::SupportsAttributeGrant(float Level) const
{
	if (!FMath::IsFinite(Level) || Level < 1.f || Level > 100000.f || DurationPolicy != EGameplayEffectDurationType::Infinite
		|| !Executions.IsEmpty() || Period.GetValueAtLevel(Level) != 0.f || StackingType != EGameplayEffectStackingType::None
		|| !GameplayCues.IsEmpty() || FindComponent<UGameplayEffectComponent>() || !GetGrantedTags().IsEmpty() || !GetBlockedAbilityTags().IsEmpty())
	{
		return false;
	}
	const FGameplayEffectSpec Spec(this, FGameplayEffectContextHandle(), Level);
	for (const FGameplayModifierInfo& Modifier : Modifiers)
	{
		float Magnitude = 0.f;
		const EGameplayEffectMagnitudeCalculation Type = Modifier.ModifierMagnitude.GetMagnitudeCalculationType();
		const bool bPureMagnitude = Type == EGameplayEffectMagnitudeCalculation::ScalableFloat
			|| (Type == EGameplayEffectMagnitudeCalculation::CustomCalculationClass
				&& Modifier.ModifierMagnitude.GetCustomMagnitudeCalculationClass() == UUEffectCalculation_Level::StaticClass());
		if (!Modifier.Attribute.IsValid() || !bPureMagnitude
			|| !Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Magnitude, false, 0.f) || !FMath::IsFinite(Magnitude)
			|| (Modifier.ModifierOp == EGameplayModOp::Division && FMath::IsNearlyZero(Magnitude)))
		{
			return false;
		}
	}
	return true;
}
