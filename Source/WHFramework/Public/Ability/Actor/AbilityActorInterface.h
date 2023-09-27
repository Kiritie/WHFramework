#pragma once

#include "AbilitySystemInterface.h"
#include "Ability/AbilityModuleTypes.h"

#include "AbilityActorInterface.generated.h"

class UAttributeSetBase;
class UAbilityBase;

UINTERFACE()
class WHFRAMEWORK_API UAbilityActorInterface : public UAbilitySystemInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityActorInterface : public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	virtual void InitializeAbilitySystem(AActor* InOwnerActor = nullptr, AActor* InAvatarActor = nullptr);

	virtual void RefreshAttributes();

	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) = 0;
	
public:
	virtual int32 GetLevelV() const = 0;

	virtual bool SetLevelV(int32 InLevel) = 0;

	virtual float GetRadius() const = 0;

	virtual float GetHalfHeight() const = 0;

	virtual UAttributeSetBase* GetAttributeSet() const = 0;
};
