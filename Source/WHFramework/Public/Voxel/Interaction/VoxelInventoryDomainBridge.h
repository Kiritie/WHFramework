#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryDomain.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"

class UInventoryWorldSubsystem;

class WHFRAMEWORK_API FVoxelInventoryDomainBridge final : public IInventoryDomainAdapter,
    public TSharedFromThis<FVoxelInventoryDomainBridge>
{
public:
    explicit FVoxelInventoryDomainBridge(UInventoryWorldSubsystem& InSystem);
    virtual ~FVoxelInventoryDomainBridge() override;
    bool Register(FString& OutError);
    bool AttachRuntime(FVoxelWorldRuntime& InRuntime, FString& OutError);
    void DetachRuntime();
    void SetPublisher(TFunction<void(const FVoxelEditBatch&)> InPublisher);
    bool BindContainer(const FGuid& InInventoryId, const FIntVector& InAnchor);
    void UnbindContainer(const FGuid& InInventoryId);
    FGuid FindInventoryAt(const FIntVector& InAnchor) const;
    bool AppendEdit(const TArray<FVoxelCellEdit>& InCells, const TArray<FVoxelEntityEdit>& InEntities,
        TArray<FInventoryDomainRedo>& InOutChanges, FString& OutError);

    virtual FName GetDomain() const override;
    virtual bool Prepare(const FGuid& InTransactionId, TConstArrayView<FInventoryDomainRedo> InChanges, FString& OutError) override;
    virtual bool ValidatePrepared(const FGuid& InTransactionId, FString& OutError) const override;
    virtual bool ApplyCommitted(const FGuid& InTransactionId, const FInventoryDomainRedo& InRedo, FString& OutError) override;
    virtual bool Restore(const FInventoryDomainRecord& InRecord, FString& OutError) override;
    virtual void Publish(const FGuid& InTransactionId) override;
    virtual void Cancel(const FGuid& InTransactionId) override;

private:
    struct FPending
    {
        FVoxelPreparedInventoryCommit Prepared;
        FVoxelEditBatch Committed;
        TArray<FGuid> EntityIds;
        bool bApplied = false;
    };

    UInventoryWorldSubsystem& System;
    FVoxelWorldRuntime* Runtime = nullptr;
    TMap<FGuid, FIntVector> ContainerAnchors;
    TMap<FGuid, TSharedPtr<FPending>> Pending;
    TFunction<void(const FVoxelEditBatch&)> Publisher;

    bool ProjectInventories(const FInventoryPlan& InPlan, TArray<FInventoryDomainRedo>& InOutChanges, FString& OutError);
    bool ReadCurrent(const FIntVector& InSection, FVoxelInventorySectionTarget& OutTarget, FString& OutError) const;
    bool PutTarget(const FVoxelInventorySectionTarget& InTarget, TArray<FInventoryDomainRedo>& InOutChanges, FString& OutError) const;
    bool ValidateRecord(TConstArrayView<uint8> InBytes, FString& OutError) const;
    bool RestoreBindings(const FVoxelPersistentSection& InSection, FString& OutError);
    FGuid SectionId(const FIntVector& InSection) const;
};
