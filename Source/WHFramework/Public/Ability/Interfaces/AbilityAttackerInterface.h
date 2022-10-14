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
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool CanHitTarget(AActor* InTarget);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void ClearHitTargets();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetHitAble(bool bValue);
};
