#include "Ability/Inventory/Widget/WidgetInventorySlotView.h"

#include "TimerManager.h"

#include "Ability/Inventory/Widget/WidgetAbilityInventoryBase.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UWidgetInventorySlotView::RebuildWidget()
{
    if (WidgetTree && WidgetTree->RootWidget && Frame)
    {
        return Super::RebuildWidget();
    }
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSize"));
    Size->SetWidthOverride(64.f);
    Size->SetHeightOverride(64.f);
    Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotFrame"));
    Frame->SetPadding(FMargin(3.f));
    UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SlotLayers"));
    Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    Overlay->AddChildToOverlay(Icon);
    Cooldown = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Cooldown"));
    Cooldown->SetFillColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.65f));
    Cooldown->SetVisibility(ESlateVisibility::Collapsed);
    Overlay->AddChildToOverlay(Cooldown);
    Count = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemCount"));
    UOverlaySlot* CountSlot = Overlay->AddChildToOverlay(Count);
    CountSlot->SetHorizontalAlignment(HAlign_Right);
    CountSlot->SetVerticalAlignment(VAlign_Bottom);
    Count->SetVisibility(ESlateVisibility::HitTestInvisible);
    Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemStatus"));
    UOverlaySlot* StatusSlot = Overlay->AddChildToOverlay(Status);
    StatusSlot->SetHorizontalAlignment(HAlign_Left);
    StatusSlot->SetVerticalAlignment(VAlign_Top);
    Status->SetVisibility(ESlateVisibility::HitTestInvisible);
    Frame->SetContent(Overlay);
    Size->SetContent(Frame);
    WidgetTree->RootWidget = Size;
    SetIsFocusable(true);
    RefreshFrame();
    if(bHasDisplay)
    {
        RefreshSlot(DisplayState, DisplayName, DisplayIcon.Get(), DisplayTooltip);
    }
    AdvanceCooldown();
    return Super::RebuildWidget();
}

void UWidgetInventorySlotView::BindSlot(UWidgetAbilityInventoryBase* InPanel, const FGuid& InInventoryId, const FInventorySlotKey& InSlot)
{
    Panel = InPanel;
    InventoryId = InInventoryId;
    Slot = InSlot;
}

void UWidgetInventorySlotView::RefreshSlot(const FInventorySlotState& InState, const FText& InName, UObject* InIcon, const FText& InTooltip)
{
    DisplayState = InState;
    DisplayName = InName;
    DisplayTooltip = InTooltip;
    DisplayIcon = InIcon;
    bHasDisplay = true;
    if (!Icon || !Count || !Status)
    {
        return;
    }
    Icon->SetVisibility(InState.Item.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (UTexture2D* Texture = Cast<UTexture2D>(InIcon))
    {
        Icon->SetBrushFromTexture(Texture);
    }
    else if (UMaterialInterface* Material = Cast<UMaterialInterface>(InIcon))
    {
        Icon->SetBrushFromMaterial(Material);
    }
    else
    {
        Icon->SetBrush(FSlateBrush());
    }
    Count->SetText(InState.Item.Count > 1 ? FText::AsNumber(InState.Item.Count) : FText::GetEmpty());
    Status->SetText(InState.bLocked ? FText::FromString(TEXT("锁")) : (InState.bFavorite ? FText::FromString(TEXT("★")) : FText::GetEmpty()));
    SetToolTipText(InState.Item.IsEmpty() ? FText::GetEmpty() : (InTooltip.IsEmpty() ? InName : InTooltip));
}

void UWidgetInventorySlotView::RefreshFrame()
{
    if (Frame)
    {
        Frame->SetBrushColor(bSelected ? FLinearColor(0.8f, 0.65f, 0.15f, 1.f) : FLinearColor(0.12f, 0.13f, 0.16f, 1.f));
        SetRenderOpacity(bPending ? 0.45f : 1.f);
    }
}

void UWidgetInventorySlotView::SetPending(bool bInPending)
{
    bPending = bInPending;
    RefreshFrame();
}

void UWidgetInventorySlotView::SetSelected(bool bInSelected)
{
    bSelected = bInSelected;
    RefreshFrame();
}

void UWidgetInventorySlotView::SetCooldown(float InRemaining, float InDuration)
{
    CooldownDuration = FMath::Max(0.f, InDuration);
    CooldownEnd = FPlatformTime::Seconds() + FMath::Max(0.f, InRemaining);
    AdvanceCooldown();
    if(GetWorld() && InRemaining > 0.f && InDuration > 0.f)
    {
        GetWorld()->GetTimerManager().SetTimer(CooldownTimer, this, &UWidgetInventorySlotView::AdvanceCooldown, 0.05f, true);
    }
}

void UWidgetInventorySlotView::AdvanceCooldown()
{
    const double Remaining = FMath::Max(0.0, CooldownEnd - FPlatformTime::Seconds());
    const bool bActive = Remaining > 0.0 && CooldownDuration > 0.f;
    if(Cooldown)
    {
        Cooldown->SetPercent(bActive ? FMath::Clamp(static_cast<float>(Remaining) / CooldownDuration, 0.f, 1.f) : 0.f);
        Cooldown->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if(!bActive && GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CooldownTimer);
}

void UWidgetInventorySlotView::NativeDestruct()
{
    if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CooldownTimer);
    SetToolTipText(FText::GetEmpty());
    Super::NativeDestruct();
}

FGuid UWidgetInventorySlotView::GetInventoryId() const
{
    return InventoryId;
}

FInventorySlotKey UWidgetInventorySlotView::GetSlotKey() const
{
    return Slot;
}

FReply UWidgetInventorySlotView::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!Panel.IsValid() || bPending)
    {
        return FReply::Unhandled();
    }
    Panel->SelectSlot(InventoryId, Slot);
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (InMouseEvent.IsControlDown())
        {
            Panel->RequestFavorite(InventoryId, Slot);
        }
        else
        {
            Panel->RequestUse(InventoryId, Slot);
        }
        return FReply::Handled();
    }
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (InMouseEvent.IsShiftDown())
        {
            const FInventorySlotState* View = Panel->FindSlot(InventoryId, Slot);
            if (View)
            {
                Panel->RequestTransfer(InventoryId, Slot, View->Item.Count);
            }
            return FReply::Handled();
        }
        bChooseQuantity = InMouseEvent.IsAltDown();
        return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UWidgetInventorySlotView::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (Panel.IsValid() && !bPending)
    {
        Panel->RequestUse(InventoryId, Slot);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UWidgetInventorySlotView::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    OutOperation = nullptr;
    const FInventorySlotState* View = Panel.IsValid() ? Panel->FindSlot(InventoryId, Slot) : nullptr;
    UInventoryNetworkComponent* Network = Panel.IsValid() ? Panel->GetNetwork() : nullptr;
    if (!Network || !Network->GetWorldEpoch().IsValid() || !Network->GetSessionId(InventoryId).IsValid() ||
        !View || View->Item.IsEmpty() || View->bLocked || bPending)
    {
        return;
    }
    UInventoryDragOperation* Drag = NewObject<UInventoryDragOperation>(this);
    Drag->Payload = Panel.Get();
    Drag->WorldEpoch = Network->GetWorldEpoch();
    Drag->SourceSessionId = Network->GetSessionId(InventoryId);
    Drag->InventoryId = InventoryId;
    Drag->Slot = Slot;
    Drag->Signature = View->Item.Signature();
    Drag->Quantity = View->Item.Count;
    Drag->bChooseQuantity = bChooseQuantity && View->Item.Count > 1 && !View->Item.InstanceId.IsValid();
    Drag->Pivot = EDragPivot::MouseDown;
    OutOperation = Drag;
}

bool UWidgetInventorySlotView::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    const UInventoryDragOperation* Drag = Cast<UInventoryDragOperation>(InOperation);
    return Drag && Panel.IsValid() && !bPending && Panel->AcceptDrop(*Drag, InventoryId, Slot);
}

FReply UWidgetInventorySlotView::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (!Panel.IsValid() || bPending)
    {
        return FReply::Unhandled();
    }
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        Panel->RequestUse(InventoryId, Slot);
    }
    else if (Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Top)
    {
        Panel->RequestDrop(InventoryId, Slot, 1);
    }
    else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
    {
        Panel->Navigate(InventoryId, Slot, -1);
    }
    else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
    {
        Panel->Navigate(InventoryId, Slot, 1);
    }
    else if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
    {
        Panel->Navigate(InventoryId, Slot, -10);
    }
    else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
    {
        Panel->Navigate(InventoryId, Slot, 10);
    }
    else
    {
        return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
    }
    return FReply::Handled();
}
