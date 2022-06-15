#pragma once

#include "AttributeSet.h"
#include "Ability/AbilityModuleTypes.h"
#include "AbilitySystemComponent.h"
#include "AttributeSetBase.generated.h"

/**
 * 属性集
 */
UCLASS()
class WHFRAMEWORK_API UAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAttributeSetBase();

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;

protected:
	/** 当MaxAttribute发生改变时按比例设置Attribute */
	virtual void AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty);

public:
	FGameplayAttributeData* GetAttributeData(const FGameplayAttribute& Attribute);

	TArray<FGameplayAttribute> GetPersistentAttributes();
};