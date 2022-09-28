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
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;

protected:
	/** 当MaxAttribute发生改变时按比例设置Attribute */
	virtual void AdjustAttributeForMaxChange(FGameplayAttributeData& InAffectedAttribute, const FGameplayAttributeData& InMaxAttribute, float InNewMaxValue, const FGameplayAttribute& InAffectedAttributeProperty);

public:
	UFUNCTION(BlueprintPure)
	FGameplayAttributeData GetAttributeData(FGameplayAttribute InAttribute);

	UFUNCTION(BlueprintPure)
	float GetAttributeValue(FGameplayAttribute InAttribute);

	UFUNCTION(BlueprintCallable)
	void SetAttributeValue(FGameplayAttribute InAttribute, float InValue);

	UFUNCTION(BlueprintCallable)
	void ModifyAttributeValue(FGameplayAttribute InAttribute, float InDeltaValue);

	UFUNCTION(BlueprintPure)
	TArray<FGameplayAttribute> GetAllAttributes() const;

	UFUNCTION(BlueprintPure)
	TArray<FGameplayAttribute> GetPersistentAttributes() const;
};
