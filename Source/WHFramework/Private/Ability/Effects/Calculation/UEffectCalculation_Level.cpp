// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Effects/Calculation/UEffectCalculation_Level.h"

float UUEffectCalculation_Level::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	return Spec.GetLevel();
}
