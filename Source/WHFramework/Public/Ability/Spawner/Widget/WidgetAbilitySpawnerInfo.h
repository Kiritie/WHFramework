#pragma once

#include "Ability/Inventory/InventoryTypes.h"
#include "Widget/World/WorldWidgetBase.h"
#include "WidgetAbilitySpawnerInfo.generated.h"

class UTextBlock;

UCLASS()
class WHFRAMEWORK_API UWidgetAbilitySpawnerInfo : public UWorldWidgetBase
{
    GENERATED_BODY()

public:
    UWidgetAbilitySpawnerInfo(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable)
    void InitAbilityItem(const FAbilityItem& InAbilityItem);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> ItemText;

private:
    FAbilityItem Item;
    void RefreshPresentation();
};
