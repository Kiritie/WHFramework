#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilityLevelValue.h"
#include "AbilityAttributeModifier.generated.h"

UENUM(BlueprintType)
enum class EAbilityAttributeOperation : uint8
{
	Add,
	Multiply
};

/** Authored attribute contribution; evaluated by the authority at the source item's level. */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityAttributeModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	EAbilityAttributeOperation Operation = EAbilityAttributeOperation::Add;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	FAbilityLevelValue Magnitude;

	bool IsValid(int32 MaxLevel) const;
	static bool ValidateList(const TArray<FAbilityAttributeModifier>& Values, int32 MaxLevel, FString& OutError);
	bool operator==(const FAbilityAttributeModifier& Other) const
	{
		return Attribute == Other.Attribute && Operation == Other.Operation &&
			Magnitude.Base == Other.Magnitude.Base && Magnitude.Growth == Other.Magnitude.Growth &&
			Magnitude.Increment == Other.Magnitude.Increment && Magnitude.Multiplier == Other.Magnitude.Multiplier;
	}
};
