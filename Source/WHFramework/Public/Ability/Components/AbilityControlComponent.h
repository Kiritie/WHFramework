#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "AbilityControlComponent.generated.h"

class UAbilitySystemComponentBase;
class UPawnMovementComponent;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityControlRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bBlockMovement = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bBlockRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bBlockAllActions = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer BlockedActions;
};

// 控制效果由 GAS 持有；组件只把标签投影为移动约束和动作准入。
UCLASS(ClassGroup = Ability, meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UAbilityControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAbilityControlComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BindAbilitySystem(UAbilitySystemComponentBase* InSystem);
	void SetAdditionalRules(const TArray<FAbilityControlRule>& InRules);
	void SetStatic(bool bEnabled);
	void AddInterrupt(float Duration);
	void ClearInterrupts();
	void ResetControls();
	bool IsActionBlocked(FGameplayTag ActionTag) const;
	bool IsMovementBlocked() const;
	bool IsRotationBlocked() const;

private:
	UPROPERTY(Transient)
	TArray<FAbilityControlRule> Rules;

	TWeakObjectPtr<UAbilitySystemComponentBase> System;
	TMap<FGameplayTag, FDelegateHandle> TagDelegates;
	FActiveGameplayEffectHandle StaticHandle;
	TWeakObjectPtr<UPawnMovementComponent> SuspendedMovement;
	bool bMovementWasActive = false;
	const FGuid MovementSource = FGuid(0x6E41E5E4, 0x7DA0443E, 0x97576D6E, 0x20D8F9D4);

	void RegisterTags();
	void UnregisterTags();
	void OnTagChanged(FGameplayTag Tag, int32 Count);
	void RefreshProjection();
	void ReleaseMovement();
	bool CanResumeMovement() const;
};
