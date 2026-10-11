#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Ability/Inventory/InventoryItemRegistry.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "InventoryItemAssetCatalog.generated.h"

class UAbilityItemDataBase;

UCLASS()
class WHFRAMEWORK_API UInventoryItemAssetCatalog : public UObject
{
    GENERATED_BODY()

public:
    bool Initialize(TConstArrayView<UAbilityItemDataBase*> InAssets, TArray<FInventoryItemDefinition>& OutDefinitions, FString& OutError);
    const UAbilityItemDataBase* Find(const FPrimaryAssetId& InItemId) const;

    template<class T>
    const T* FindAs(const FPrimaryAssetId& InItemId) const
    {
        return Cast<T>(Find(InItemId));
    }

    UObject* FindIcon(const FPrimaryAssetId& InItemId) const;
    bool IsInitialized() const;

private:
    UPROPERTY(Transient)
    TMap<FPrimaryAssetId, TObjectPtr<UAbilityItemDataBase>> Assets;

    bool bInitialized = false;
};
