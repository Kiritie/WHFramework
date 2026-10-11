#include "Ability/Inventory/InventoryItemAssetCatalog.h"

#include "Ability/Item/AbilityItemDataBase.h"

bool UInventoryItemAssetCatalog::Initialize(TConstArrayView<UAbilityItemDataBase*> InAssets, TArray<FInventoryItemDefinition>& OutDefinitions, FString& OutError)
{
    OutDefinitions.Reset();
    if (bInitialized || InAssets.IsEmpty())
    {
        OutError = TEXT("物品资产目录已初始化或输入为空");
        return false;
    }
    TMap<FPrimaryAssetId, TObjectPtr<UAbilityItemDataBase>> Candidate;
    TArray<FInventoryItemDefinition> Definitions;
    for (UAbilityItemDataBase* Asset : InAssets)
    {
        if (!IsValid(Asset))
        {
            OutError = TEXT("物品资产目录包含无效资产");
            return false;
        }
        FInventoryItemDefinition Definition = Asset->BuildInventoryDefinition();
        if (!Definition.ItemId.IsValid() || Candidate.Contains(Definition.ItemId))
        {
            OutError = FString::Printf(TEXT("物品资产身份无效或重复: %s"), *Definition.ItemId.ToString());
            return false;
        }
        Candidate.Add(Definition.ItemId, Asset);
        Definitions.Add(MoveTemp(Definition));
    }
    Definitions.Sort([](const FInventoryItemDefinition& Left, const FInventoryItemDefinition& Right)
    {
        return Left.ItemId.ToString() < Right.ItemId.ToString();
    });
    FInventoryItemRegistry Validator;
    if (!Validator.Initialize(Definitions, OutError))
    {
        return false;
    }
    Assets = MoveTemp(Candidate);
    OutDefinitions = MoveTemp(Definitions);
    bInitialized = true;
    return true;
}

const UAbilityItemDataBase* UInventoryItemAssetCatalog::Find(const FPrimaryAssetId& InItemId) const
{
    const TObjectPtr<UAbilityItemDataBase>* Asset = Assets.Find(InItemId);
    return Asset ? Asset->Get() : nullptr;
}

UObject* UInventoryItemAssetCatalog::FindIcon(const FPrimaryAssetId& InItemId) const
{
    const UAbilityItemDataBase* Asset = Find(InItemId);
    return Asset ? Asset->Icon : nullptr;
}

bool UInventoryItemAssetCatalog::IsInitialized() const
{
    return bInitialized;
}
