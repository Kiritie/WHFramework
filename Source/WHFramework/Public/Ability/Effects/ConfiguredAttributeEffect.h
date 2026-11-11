#pragma once

#include "Ability/AbilityAttributeModifier.h"
#include "Ability/Effects/EffectBase.h"
#include "ConfiguredAttributeEffect.generated.h"

/** Stable native GE layout. GAS replicates evaluated spec modifiers; SetByCaller inputs remain authoritative. */
UCLASS(Abstract)
class WHFRAMEWORK_API UConfiguredAttributeEffect : public UEffectBase
{
	GENERATED_BODY()

public:
	UConfiguredAttributeEffect();

	bool ConfigureSpec(FGameplayEffectSpec& Spec, const TArray<FAbilityAttributeModifier>& Values, int32 Level) const;

protected:
	/** Call from the concrete native effect constructor with a deterministic attribute list. */
	void InitializeAttributes(const TArray<FGameplayAttribute>& Attributes);

private:
	static FName GetMagnitudeName(int32 ModifierIndex);
};
