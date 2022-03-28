#pragma once

#include "Abilities/GameplayAbility.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/AbilityModuleTypes.h"
#include "AbilityBase.generated.h"

/**
 * GameplayAbility基类
 */
UCLASS()
class WHFRAMEWORK_API UAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UAbilityBase();

public:
	/** GameplayEffectr容器列表 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FGameplayEffectContainer> EffectContainerMap;

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

public:
	/** 从GameplayEffect容器创建GameplayEffect容器规范 */
	UFUNCTION(BlueprintCallable, Category = "Ability", meta = (AutoCreateRefTerm = "EventData"))
	virtual FGameplayEffectContainerSpec MakeEffectContainerSpecFromContainer(const FGameplayEffectContainer& Container, const FGameplayEventData& EventData, int32 OverrideGameplayLevel = -1);

	/** 从标签容器创建GameplayEffect容器规范 */
	UFUNCTION(BlueprintCallable, Category = "Ability", meta = (AutoCreateRefTerm = "EventData"))
	virtual FGameplayEffectContainerSpec MakeEffectContainerSpec(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel = -1);

	/** 应用GameplayEffect容器规范 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual TArray<FActiveGameplayEffectHandle> ApplyEffectContainerSpec(const FGameplayEffectContainerSpec& ContainerSpec);

	/** 应用GameplayEffect容器 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual TArray<FActiveGameplayEffectHandle> ApplyEffectContainer(FGameplayTag ContainerTag, const FGameplayEventData& EventData,	int32 OverrideGameplayLevel = -1);
};