#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "WidgetInventorySlotView.generated.h"

class UCommonImageN;
class UCommonTextBlockN;
class UCommonProgress;
class UWidgetAbilityInventoryBase;

UCLASS()
class WHFRAMEWORK_API UInventoryDragOperation : public UDragDropOperation
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FGuid WorldEpoch;

    UPROPERTY()
    FGuid SourceSessionId;

    UPROPERTY()
    FGuid InventoryId;

    UPROPERTY()
    FInventorySlotKey Slot;

    UPROPERTY()
    FInventoryStackSignature Signature;

    UPROPERTY()
    int32 Quantity = 0;

    UPROPERTY()
    bool bChooseQuantity = false;
};

UCLASS()
class WHFRAMEWORK_API UWidgetInventorySlotView : public UUserWidget
{
    GENERATED_BODY()

public:
    UWidgetInventorySlotView(const FObjectInitializer& ObjectInitializer);
    void BindSlot(UWidgetAbilityInventoryBase* InPanel, const FGuid& InInventoryId, const FInventorySlotKey& InSlot);
    virtual void RefreshSlot(const FInventorySlotState& InState, const FText& InName, UObject* InIcon, const FText& InTooltip);
    void SetPending(bool bInPending);
    void SetSelected(bool bInSelected);
    void SetCooldown(float InRemaining, float InDuration);
    FGuid GetInventoryId() const;
    FInventorySlotKey GetSlotKey() const;
    UWidgetAbilityInventoryBase* GetInventoryPanel() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "UI.Style.Brush"))
    FGameplayTag FrameStyleTag;

    UPROPERTY(EditDefaultsOnly, meta = (Categories = "UI.Style.Brush"))
    FGameplayTag LockedStyleTag;

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonImageN> Frame;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonImageN> Icon;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlockN> Count;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlockN> Status;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonProgress> Cooldown;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonImageN> SelectionMarker;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonImageN> PendingMarker;

    TWeakObjectPtr<UWidgetAbilityInventoryBase> Panel;
    FGuid InventoryId;
    FInventorySlotKey Slot;
    bool bPending = false;
    bool bSelected = false;
    bool bChooseQuantity = false;
    FInventorySlotState DisplayState;
    FText DisplayName;
    FText DisplayTooltip;
    TWeakObjectPtr<UObject> DisplayIcon;
    bool bHasDisplay = false;
    double CooldownEnd = 0.0;
    float CooldownDuration = 0.f;
    FTimerHandle CooldownTimer;
    void AdvanceCooldown();
    void RefreshFrame();
};
