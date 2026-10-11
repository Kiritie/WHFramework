#pragma once

#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "ItemAbilityBase.generated.h"

class UEffectBase;

UCLASS()
class WHFRAMEWORK_API UItemAbilityBase : public UAbilityBase
{
    GENERATED_BODY()

public:
    UItemAbilityBase();
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool GetInventoryItem(FAbilityItem& OutItem) const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffects")
    TArray<TSubclassOf<UEffectBase>> EffectClasses;

    virtual TArray<TSubclassOf<UEffectBase>> GetEffectClasses() const override;

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    TArray<FActiveGameplayEffectHandle> EffectHandles;
};
