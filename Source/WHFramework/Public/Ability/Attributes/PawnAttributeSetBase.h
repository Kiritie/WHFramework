#pragma once

#include "VitalityAttributeSetBase.h"
#include "Ability/AbilityModuleTypes.h"

#include "PawnAttributeSetBase.generated.h"

class UDamageHandle;

/**
 * 生命属性集
 */
UCLASS()
class WHFRAMEWORK_API UPawnAttributeSetBase : public UVitalityAttributeSetBase
{
	GENERATED_BODY()

public:
	UPawnAttributeSetBase();

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
