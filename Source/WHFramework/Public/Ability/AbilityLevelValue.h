#pragma once

#include "CoreMinimal.h"
#include "AbilityLevelValue.generated.h"

UENUM(BlueprintType)
enum class EAbilityValueGrowth : uint8
{
	None,
	Linear,
	Exponential
};

// 等级 1 使用基数；None 表示固定数值，不需要额外曲线或数据表。
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityLevelValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Base = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilityValueGrowth Growth = EAbilityValueGrowth::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Growth == EAbilityValueGrowth::Linear", EditConditionHides))
	float Increment = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Growth == EAbilityValueGrowth::Exponential", EditConditionHides, ClampMin = "0.0001"))
	float Multiplier = 1.f;

	float Evaluate(int32 InLevel) const;
	bool IsValid(int32 InMaxLevel) const;
	static FAbilityLevelValue Fixed(float Value)
	{
		FAbilityLevelValue Result;
		Result.Base = Value;
		return Result;
	}
};
