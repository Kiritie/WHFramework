#pragma once

#include "Ability/Base/AttributeSetBase.h"

#include "VitalityAttributeSetBase.generated.h"

/**
 * 角色属性集
 */
UCLASS()
class WHFRAMEWORK_API UVitalityAttributeSetBase : public UAttributeSetBase
{
	GENERATED_BODY()

public:
	UVitalityAttributeSetBase();

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;
};