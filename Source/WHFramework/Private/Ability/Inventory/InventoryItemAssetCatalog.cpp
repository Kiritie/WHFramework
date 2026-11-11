#include "Ability/Inventory/InventoryItemAssetCatalog.h"

#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Inventory/InventoryPickupActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

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
        if (Asset->MeshData.Mesh && !Cast<UStaticMesh>(Asset->MeshData.Mesh) && !Cast<USkeletalMesh>(Asset->MeshData.Mesh))
        {
            OutError = TEXT("物品网格必须是静态网格或骨骼网格: ") + Asset->GetPathName();
            return false;
        }
        if (Asset->MeshData.LocalTransform.ContainsNaN() ||
            (Asset->MeshData.AnimationClass && !Cast<USkeletalMesh>(Asset->MeshData.Mesh)))
        {
            OutError = TEXT("物品网格变换或动画类型无效: ") + Asset->GetPathName();
            return false;
        }
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

