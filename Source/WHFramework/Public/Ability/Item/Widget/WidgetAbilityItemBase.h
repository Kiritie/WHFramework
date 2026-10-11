#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Widget/Sub/SubButtonWidgetBase.h"
#include "WidgetAbilityItemBase.generated.h"

class UAbilityItemDataBase;

/** 物品展示值不持有槽位、背包或运行时宿主。 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetAbilityItemBase : public USubButtonWidgetBase
{
    GENERATED_BODY()

public:
    UWidgetAbilityItemBase(const FObjectInitializer& ObjectInitializer);

    //////////////////////////////////////////////////////////////////////////
    /// ObjectPool
public:
    virtual void OnSpawn_Implementation(const FParameter& InParam) override;
    virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

public:
    virtual void OnCreate(const FParameter& InParam) override;
    virtual void OnInitialize(const FParameter& InParam) override;
    virtual void OnRefresh() override;
    virtual void OnDestroy(EObjectDespawnMode InMode) override;

    void SetItem(const FAbilityItem& InItem);
    FAbilityItem GetItem() const { return Item; }

protected:
    const UAbilityItemDataBase* FindItemData() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FAbilityItem Item;
};
