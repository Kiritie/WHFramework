#pragma once

#include "CoreMinimal.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "Ability/Inventory/InventoryNetworkComponent.h"
#include "WidgetAbilityInventoryBase.generated.h"

class UVerticalBox;
class UTextBlock;
class USpinBox;
class UHorizontalBox;
class UWidgetInventorySlotView;
class UInventoryDragOperation;

DECLARE_MULTICAST_DELEGATE_TwoParams(FInventoryUISelection, const FGuid&, const FInventorySlotKey&);

UCLASS()
class WHFRAMEWORK_API UWidgetAbilityInventoryBase : public UUserWidgetBase
{
    GENERATED_BODY()

public:
    UWidgetAbilityInventoryBase(const FObjectInitializer& ObjectInitializer);
    virtual void OnInitialize(const FParameter& InParams) override;
    virtual void OnRefresh() override;
    virtual void OnClose(bool bInstant) override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintCallable)
    void BindInventories(UInventoryNetworkComponent* InNetwork, const TArray<FGuid>& InInventoryIds, bool bInFollowOwner = false);

    UFUNCTION(BlueprintCallable)
    void UnbindInventories();

    UFUNCTION(BlueprintCallable)
    void SelectSlot(FGuid InInventoryId, FInventorySlotKey InSlot);

    UFUNCTION(BlueprintCallable)
    bool RequestUse(FGuid InInventoryId, FInventorySlotKey InSlot);

    UFUNCTION(BlueprintCallable)
    bool RequestDrop(FGuid InInventoryId, FInventorySlotKey InSlot, int32 InQuantity);

    UFUNCTION(BlueprintCallable)
    bool RequestTransfer(FGuid InInventoryId, FInventorySlotKey InSlot, int32 InQuantity);

    UFUNCTION(BlueprintCallable)
    bool RequestSort(FGuid InInventoryId, FGameplayTag InGroup);

    UFUNCTION(BlueprintCallable)
    bool RequestFavorite(FGuid InInventoryId, FInventorySlotKey InSlot);

    UFUNCTION(BlueprintCallable)
    bool RequestQuickStack(FGuid InInventoryId);

    bool AcceptDrop(const UInventoryDragOperation& InDrag, const FGuid& InTargetInventoryId, const FInventorySlotKey& InTargetSlot);
    const FInventorySlotState* FindSlot(const FGuid& InInventoryId, const FInventorySlotKey& InSlot) const;
    UInventoryNetworkComponent* GetNetwork() const;
    FGuid GetSelectedInventoryId() const;
    FInventorySlotKey GetSelectedSlotKey() const;
    void Navigate(const FGuid& InInventoryId, const FInventorySlotKey& InSlot, int32 InOffset);
    void UpdateCooldown(const FGuid& InInventoryId, const FInventorySlotKey& InSlot, float InRemaining, float InDuration);
    FInventoryUISelection OnSelection;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UWidgetInventorySlotView> SlotWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName UseAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName DropAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Columns = 10;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FGameplayTag> VisibleGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, FText> GroupNames;

private:
    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> InventoryContent;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ResultText;

    UPROPERTY(Transient)
    TObjectPtr<UHorizontalBox> SplitControls;

    UPROPERTY(Transient)
    TObjectPtr<USpinBox> SplitCount;

    TWeakObjectPtr<UInventoryNetworkComponent> Network;
    TArray<FGuid> InventoryIds;
    TMap<FString, TWeakObjectPtr<UWidgetInventorySlotView>> Slots;
    TMap<uint64, TArray<FString>> PendingSlots;
    FInventoryClientRequest SplitRequest;
    FInventoryStackSignature SplitSignature;
    FGuid SplitWorldEpoch;
    FGuid SplitSourceSessionId;
    FGuid SplitTargetSessionId;
    FGuid FollowedOwnerId;
    bool bFollowOwner = false;
    FGuid SelectedInventoryId;
    FInventorySlotKey SelectedSlot;
    FDelegateHandle SnapshotHandle;
    FDelegateHandle DeltaHandle;
    FDelegateHandle ResultHandle;
    FDelegateHandle ClosedHandle;

    static FString SlotId(const FGuid& InInventoryId, const FInventorySlotKey& InSlot);
    void RebuildViews();
    void RefreshSlot(const FGuid& InInventoryId, const FInventorySlotState& InSlot);
    void HandleSnapshot(const FInventorySnapshot& InSnapshot);
    void HandleDelta(const FInventoryDelta& InDelta);
    void HandleResult(const FInventoryCommandResult& InResult);
    void HandleClosed(const FGuid& InInventoryId, EInventoryResultCode InReason);
    bool Submit(FInventoryClientRequest InRequest);
    void RefreshPending();

    UFUNCTION()
    void ConfirmSplit();

    UFUNCTION()
    void SortSelected();

    UFUNCTION()
    void QuickStackSelected();

    UFUNCTION()
    void CancelSplit();
};
