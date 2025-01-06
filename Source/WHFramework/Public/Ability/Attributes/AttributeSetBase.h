#pragma once

#include "AttributeSet.h"
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
	virtual void SerializeAttributes(FArchive& Ar);

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;

protected:
	virtual void AdjustAttributeForMaxChange(const FGameplayAttribute& Attribute, float OldMaxValue, float NewMaxValue);

public:
	UFUNCTION(BlueprintPure)
	FGameplayAttributeData GetAttributeData(FGameplayAttribute InAttribute);

	UFUNCTION(BlueprintPure)
	float GetAttributeValue(FGameplayAttribute InAttribute);

	UFUNCTION(BlueprintPure)
	float GetAttributeBaseValue(FGameplayAttribute InAttribute);

	UFUNCTION(BlueprintCallable)
	void SetAttributeValue(FGameplayAttribute InAttribute, float InValue);

	UFUNCTION(BlueprintCallable)
	void ModifyAttributeValue(FGameplayAttribute InAttribute, float InDeltaValue);

	UFUNCTION(BlueprintCallable)
	void MultipleAttributeValue(FGameplayAttribute InAttribute, float InMultipleValue);

	UFUNCTION(BlueprintPure)
	TArray<FGameplayAttribute> GetAllAttributes() const;

	UFUNCTION(BlueprintPure)
	TArray<FGameplayAttribute> GetPersistentAttributes() const;

public:
	template<class T>
	T* GetOwnerActor() const
	{
		return Cast<T>(GetOwnerActor());
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	AActor* GetOwnerActor(TSubclassOf<AActor> InClass = nullptr) const;
};
