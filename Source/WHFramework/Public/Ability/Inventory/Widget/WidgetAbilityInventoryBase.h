#pragma once

#include "CoreMinimal.h"
#include "Widget/Sub/SubWidgetBase.h"
#include "Ability/Inventory/InventoryNetworkComponent.h"
#include "WidgetAbilityInventoryBase.generated.h"

class UVerticalBox;
class UTextBlock;
class UHorizontalBox;
class UPanelWidget;
class UCommonButton;
class UCommonRange;
class UCommonTextBlockN;
class UWidgetAbilityInventorySlotBase;
class UInventoryDragOperation;

DECLARE_MULTICAST_DELEGATE_TwoParams(FInventoryUISelection, const FGuid&, const FInventorySlotKey&);

UCLASS()
class WHFRAMEWORK_API UWidgetAbilityInventoryBase : public USubWidgetBase
{
    GENERATED_BODY()

public:
    UWidgetAbilityInventoryBase(const FObjectInitializer& ObjectInitializer);
    virtual void OnInitialize(const FParameter& InParams) override;
    virtual void OnRefresh() override;
    virtual void OnReset(bool bForce = false) override;
    virtual void OnDestroy(EObjectDespawnMode InMode) override;
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

    UFUNCTION(BlueprintPure)
    bool CanSplit(FGuid InInventoryId, FInventorySlotKey InSlot) const;

    UFUNCTION(BlueprintCallable)
    bool RequestSplit(FGuid InInventoryId, FInventorySlotKey InSlot);

    bool AcceptDrop(const UInventoryDragOperation& InDrag, const FGuid& InTargetInventoryId, const FInventorySlotKey& InTargetSlot);
    const FInventorySlotState* FindSlot(const FGuid& InInventoryId, const FInventorySlotKey& InSlot) const;
    UInventoryNetworkComponent* GetNetwork() const;
    FGuid GetSelectedInventoryId() const;
    FInventorySlotKey GetSelectedSlotKey() const;
    void Navigate(const FGuid& InInventoryId, const FInventorySlotKey& InSlot, int32 InOffset);
    void UpdateCooldown(const FGuid& InInventoryId, const FInventorySlotKey& InSlot, float InRemaining, float InDuration);
    FInventoryUISelection OnSelection;

    void SetTransferInventoryId(FGuid InInventoryId);
    void SyncSelection(FGuid InInventoryId, FInventorySlotKey InSlot);
    bool SubmitRequest(FInventoryClientRequest InRequest);
    void SetResultText(const FText& InText);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void RebuildViews();
    virtual void ConfigureSlot(UWidgetAbilityInventorySlotBase* InWidget, const FInventorySlotKey& InSlot) const;
    virtual int32 GetInventoryColumns(const FGuid& InInventoryId, const FGameplayTag& InGroup) const;
    virtual UPanelWidget* GetInventoryHost(const FGuid& InInventoryId) const;
    void ApplySelection(const FGuid& InInventoryId, const FInventorySlotKey& InSlot);
    bool Submit(FInventoryClientRequest InRequest);
	virtual bool ShowSplitQuantity(int32 InStackCount);
	virtual void CloseSplitQuantity();
	void ConfirmSplitQuantity(int32 InQuantity);
	void CancelSplit();

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bStackInventoriesVertically = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bShowInventoryTitles = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bShowGroupTitles = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, FName> GroupPanelNames;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, FName> SlotHostPrefixes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, int32> GroupColumns;

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "UI.Style.Text"))
    FGameplayTag InventoryTitleStyleTag;

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "UI.Style.Text"))
    FGameplayTag GroupTitleStyleTag;

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "UI.Style.Button"))
    FGameplayTag SecondaryButtonStyleTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UWidgetAbilityInventorySlotBase> SlotWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName UseAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName DropAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Columns = 10;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    FVector2D GridSlotSize = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FMargin GridSlotPadding = FMargin(2.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FGameplayTag> VisibleGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, FText> GroupNames;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> InventoryContent;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlockN> ResultText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonButton> Btn_Sort;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonButton> Btn_QuickStack;

private:
    TWeakObjectPtr<UInventoryNetworkComponent> Network;
    TArray<FGuid> InventoryIds;
    FGuid TransferInventoryId;
    TMap<FString, TWeakObjectPtr<UWidgetAbilityInventorySlotBase>> Slots;
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
    FDelegateHandle QueuedHandle;
    FDelegateHandle ClosedHandle;

    static FString SlotId(const FGuid& InInventoryId, const FInventorySlotKey& InSlot);
    uint64 ClearViews();
    TArray<TWeakObjectPtr<UWidget>> GeneratedViewRoots;
    uint64 ViewRevision = 0;
    void RefreshSlot(const FGuid& InInventoryId, const FInventorySlotState& InSlot);
    void HandleSnapshot(const FInventorySnapshot& InSnapshot);
    void HandleDelta(const FInventoryDelta& InDelta);
    void HandleResult(const FInventoryCommandResult& InResult);
    void HandleRequestQueued(const FInventoryClientRequest& InRequest);
    TArray<FString> CollectRequestSlots(const FInventoryClientRequest& InRequest) const;
    void HandleClosed(const FGuid& InInventoryId, EInventoryResultCode InReason);
    void RefreshPending();
    void BindControls();
    void UnbindControls();
    void RefreshControls();

    bool OpenSplit(const FInventoryClientRequest& InRequest);

    UFUNCTION()
    void SortSelected();

    UFUNCTION()
    void QuickStackSelected();

};
