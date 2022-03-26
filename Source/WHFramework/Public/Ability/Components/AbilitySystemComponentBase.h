#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystemComponentBase.generated.h"

class UAbilityBase;

/**
 * AbilitySystem组件
 */
UCLASS()
class WHFRAMEWORK_API UAbilitySystemComponentBase : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UAbilitySystemComponentBase();

	/** 根据标签获取当前激活的Ability列表 */
	void GetActiveAbilitiesWithTags(const FGameplayTagContainer & AbilityTags, TArray<UAbilityBase*>& ActiveAbilities);

	/** 获取Ability的默认等级，由角色派生 */
	int32 GetDefaultAbilityLevel() const;

public:
	/** 通过Actor获取AbilitySystemComponent */
	static UAbilitySystemComponentBase* GetAbilitySystemComponentFormActor(const AActor* Actor, bool LookForComponent = false);
};