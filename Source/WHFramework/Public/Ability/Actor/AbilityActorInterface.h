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
	void InitializeAbilities(AActor* InOwnerActor = nullptr, AActor* InAvatarActor = nullptr);

	void RefreshAttributes();

	bool AttachActor(AActor* InActor, const FAttachmentTransformRules& InAttachmentRules, FName InSocketName = NAME_None);

	void DetachActor(AActor* InActor, const FDetachmentTransformRules& InDetachmentRules);

public:
	virtual float GetExp() const = 0;

	virtual float GetMaxExp() const = 0;
	
	virtual void ModifyExp(float InDeltaValue) = 0;

protected:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) = 0;
	
	virtual void OnActorAttached(AActor* InActor) = 0;

	virtual void OnActorDetached(AActor* InActor) = 0;

public:
	virtual FName GetNameA() const = 0;

	virtual void SetNameA(FName InName) = 0;

	virtual int32 GetLevelA() const = 0;

	virtual bool SetLevelA(int32 InLevel) = 0;

	virtual float GetRadius() const = 0;

	virtual float GetHalfHeight() const = 0;

	virtual float GetDistance(AActor* InTargetActor, bool bIgnoreRadius = true, bool bIgnoreZAxis = true) const = 0;

	virtual FTransform GetBirthTransform() const = 0;

	virtual UAttributeSetBase* GetAttributeSet() const = 0;

	virtual UShapeComponent* GetCollisionComponent() const = 0;

	virtual UMeshComponent* GetMeshComponent() const = 0;

private:
	bool bAbilitiesInitialized = false;
};
