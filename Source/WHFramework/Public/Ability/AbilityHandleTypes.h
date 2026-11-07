#pragma once

#include "CoreMinimal.h"
#include "Common/Base/WHObject.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"

#include "AbilityHandleTypes.generated.h"

UCLASS(Blueprintable)
class WHFRAMEWORK_API UDamageHandle : public UWHObject
{
	GENERATED_BODY()

public:
	UDamageHandle() {}

	virtual void HandleDamage(AActor* SourceActor, AActor* TargetActor, float DamageValue, const FGameplayAttribute& DamageAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
};

/**
 * 伤害处理类
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API URecoveryHandle : public UWHObject
{
	GENERATED_BODY()

public:
	URecoveryHandle() {}

	virtual void HandleRecovery(AActor* SourceActor, AActor* TargetActor, float RecoveryValue, const FGameplayAttribute& RecoveryAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
};

/**
 * 伤害处理类
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API UInterruptHandle : public UWHObject
{
	GENERATED_BODY()

public:
	UInterruptHandle() {}

	virtual void HandleInterrupt(AActor* SourceActor, AActor* TargetActor, float InterruptDuration, const FGameplayAttribute& InterruptAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags);
};
