#pragma once

#include "GameplayModMagnitudeCalculation.h"
#include "UEffectCalculation_Constant.generated.h"

/** 纯数值幅度基元，供曲线系数与固定偏移组合使用。 */
UCLASS()
class WHFRAMEWORK_API UUEffectCalculation_Constant : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};
