#pragma once

#include "UObject/Interface.h"
#include "AbilityHitterInterface.generated.h"

UINTERFACE()
class WHFRAMEWORK_API UAbilityHitterInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityHitterInterface
{
	GENERATED_BODY()

public:
	virtual bool CanHitTarget(AActor* InTarget) const { return true; }

	virtual void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult) { }

public:
	virtual bool IsHitAble() const = 0;

	virtual void SetHitAble(bool bValue) = 0;

	virtual void ClearHitTargets() = 0;

	virtual TArray<AActor*> GetHitTargets() const = 0;
};
