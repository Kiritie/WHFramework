#pragma once

#include "UObject/Interface.h"
#include "AbilityAttackerInterface.generated.h"

UINTERFACE()
class WHFRAMEWORK_API UAbilityAttackerInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityAttackerInterface
{
	GENERATED_BODY()

public:
	virtual bool CanHitTarget(AActor* InTarget) const = 0;

	virtual void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult) = 0;
	
	virtual void ClearHitTargets() = 0;

	virtual void SetHitAble(bool bValue) = 0;
};
