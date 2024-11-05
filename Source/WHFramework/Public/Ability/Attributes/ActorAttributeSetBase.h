#pragma once

#include "AbilitySystemComponent.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Attributes/AttributeSetBase.h"

#include "ActorAttributeSetBase.generated.h"

/**
 * 生命属性集
 */
UCLASS()
class WHFRAMEWORK_API UActorAttributeSetBase : public UAttributeSetBase
{
	GENERATED_BODY()

public:
	UActorAttributeSetBase();

public:
	virtual void SerializeAttributes(FArchive& Ar) override;

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_Exp, Category = "ActorAttributes")
	FGameplayAttributeData Exp;
	GAMEPLAYATTRIBUTE_ACCESSORS(UActorAttributeSetBase, Exp)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_MaxExp, Category = "ActorAttributes")
	FGameplayAttributeData MaxExp;
	GAMEPLAYATTRIBUTE_ACCESSORS(UActorAttributeSetBase, MaxExp)

public:
	UFUNCTION()
	virtual void OnRep_Exp(const FGameplayAttributeData& OldExp);

	UFUNCTION()
	virtual void OnRep_MaxExp(const FGameplayAttributeData& OldMaxExp);
};
