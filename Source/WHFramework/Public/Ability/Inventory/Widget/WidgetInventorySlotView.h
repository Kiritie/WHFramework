#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "WidgetInventorySlotView.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UProgressBar;
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
    void BindSlot(UWidgetAbilityInventoryBase* InPanel, const FGuid& InInventoryId, const FInventorySlotKey& InSlot);
    void RefreshSlot(const FInventorySlotState& InState, const FText& InName, UObject* InIcon, const FText& InTooltip);
    void SetPending(bool bInPending);
    void SetSelected(bool bInSelected);
    void SetCooldown(float InRemaining, float InDuration);
    FGuid GetInventoryId() const;
    FInventorySlotKey GetSlotKey() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UBorder> Frame;

    UPROPERTY(Transient)
    TObjectPtr<UImage> Icon;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Count;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Status;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> Cooldown;

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
