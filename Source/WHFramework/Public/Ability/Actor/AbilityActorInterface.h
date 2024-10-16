#pragma once

#include "AbilitySystemInterface.h"
#include "Ability/AbilityModuleTypes.h"

#include "AbilityActorInterface.generated.h"

class UAttributeSetBase;
class UAbilityBase;
class UShapeComponent;

UINTERFACE()
class WHFRAMEWORK_API UAbilityActorInterface : public UAbilitySystemInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityActorInterface : public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	virtual void InitializeAbilities(AActor* InOwnerActor = nullptr, AActor* InAvatarActor = nullptr);

	virtual void RefreshAttributes();

protected:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) = 0;
	
public:
	virtual int32 GetLevelA() const = 0;

	virtual bool SetLevelA(int32 InLevel) = 0;

	virtual float GetRadius() const = 0;

	virtual float GetHalfHeight() const = 0;

	virtual UAttributeSetBase* GetAttributeSet() const = 0;

	virtual UShapeComponent* GetCollisionComponent() const = 0;

private:
	bool bAbilityInitialized = false;
};
