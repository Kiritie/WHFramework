#include "Ability/Inventory/Widget/WidgetAbilityInventorySlotBase.h"

#include "TimerManager.h"

#include "Ability/Inventory/Widget/WidgetAbilityInventoryBase.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Widget/Common/CommonImageN.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonProgress.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "InputCoreTypes.h"

UWidgetAbilityInventorySlotBase::UWidgetAbilityInventorySlotBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);
    FrameStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Brush);
    LockedStyleTag = FrameStyleTag;
}

TSharedRef<SWidget> UWidgetAbilityInventorySlotBase::RebuildWidget()
{
    if (IsDesignTime() && WidgetTree && WidgetTree->RootWidget)
    {
        return Super::RebuildWidget();
    }
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }
    if (!WidgetTree->RootWidget)
    {
        UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SlotLayers"));
        Layers->SetVisibility(ESlateVisibility::Visible);
        WidgetTree->RootWidget = Layers;
        const auto AddLayer = [Layers](UWidget* Widget)
        {
            Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
            UOverlaySlot* LayerSlot = Layers->AddChildToOverlay(Widget);
            LayerSlot->SetHorizontalAlignment(HAlign_Fill);
            LayerSlot->SetVerticalAlignment(VAlign_Fill);
            return LayerSlot;
        };
        Frame = WidgetTree->ConstructWidget<UCommonImageN>(UCommonImageN::StaticClass(), TEXT("Frame"));
        AddLayer(Frame);
        Icon = WidgetTree->ConstructWidget<UCommonImageN>(UCommonImageN::StaticClass(), TEXT("Icon"));
        IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IconSize"));
        UScaleBox* IconScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("IconScale"));
        IconScale->SetStretch(EStretch::ScaleToFit);
        IconScale->SetContent(Icon);
        IconSize->SetContent(IconScale);
        UOverlaySlot* IconSlot = AddLayer(IconSize);
        IconSlot->SetHorizontalAlignment(HAlign_Center);
        IconSlot->SetVerticalAlignment(VAlign_Center);
        Cooldown = WidgetTree->ConstructWidget<UCommonProgress>(UCommonProgress::StaticClass(), TEXT("Cooldown"));
        AddLayer(Cooldown);
        SelectionMarker = WidgetTree->ConstructWidget<UCommonImageN>(UCommonImageN::StaticClass(), TEXT("SelectionMarker"));
        SelectionMarker->SetStyleTag(WidgetStyleTags::CommonDefault(EWidgetStyleKind::Brush));
        AddLayer(SelectionMarker);
        PendingMarker = WidgetTree->ConstructWidget<UCommonImageN>(UCommonImageN::StaticClass(), TEXT("PendingMarker"));
        PendingMarker->SetStyleTag(WidgetStyleTags::CommonDefault(EWidgetStyleKind::Brush));
        AddLayer(PendingMarker);
        Count = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Count"));
        UOverlaySlot* CountSlot = AddLayer(Count);
        CountSlot->SetHorizontalAlignment(HAlign_Right);
        CountSlot->SetVerticalAlignment(VAlign_Bottom);
        Status = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Status"));
        UOverlaySlot* StatusSlot = AddLayer(Status);
        StatusSlot->SetHorizontalAlignment(HAlign_Left);
        StatusSlot->SetVerticalAlignment(VAlign_Top);
    }
    TSharedRef<SWidget> Result = Super::RebuildWidget();
    check(Frame && Icon && IconSize && Count && Status && Cooldown && SelectionMarker && PendingMarker);
    IconSize->SetWidthOverride(IconExtent.X);
    IconSize->SetHeightOverride(IconExtent.Y);
    Icon->SetStyleTag(FGameplayTag());
    if (!bHasDisplay)
    {
        Icon->SetVisibility(ESlateVisibility::Collapsed);
    }
    RefreshFrame();
    if(bHasDisplay)
    {
        RefreshSlot(DisplayState, DisplayName, DisplayIcon.Get(), DisplayTooltip);
    }
    AdvanceCooldown();
    return Result;
}

void UWidgetAbilityInventorySlotBase::BindSlot(UWidgetAbilityInventoryBase* InPanel, const FGuid& InInventoryId, const FInventorySlotKey& InSlot)
{
    Panel = InPanel;
    InventoryId = InInventoryId;
    Slot = InSlot;
}

void UWidgetAbilityInventorySlotBase::SetPresentation(FGameplayTag InFrameStyle, FGameplayTag InLockedStyle, FVector2D InIconExtent)
{
    FrameStyleTag = InFrameStyle;
    LockedStyleTag = InLockedStyle;
    IconExtent = InIconExtent;
    if (IconSize)
    {
        IconSize->SetWidthOverride(IconExtent.X);
        IconSize->SetHeightOverride(IconExtent.Y);
    }
    RefreshFrame();
}

void UWidgetAbilityInventorySlotBase::RefreshSlot(const FInventorySlotState& InState, const FText& InName, UObject* InIcon, const FText& InTooltip)
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
    Status->SetText(InState.bLocked ? NSLOCTEXT("WH.Inventory", "Locked", "锁定")
        : InState.bFavorite ? NSLOCTEXT("WH.Inventory", "Favorite", "收藏") : FText::GetEmpty());
    RefreshFrame();
    if (!GetToolTip())
    {
        SetToolTipText(InState.Item.IsEmpty() ? FText::GetEmpty() : (InTooltip.IsEmpty() ? InName : InTooltip));
    }
}

void UWidgetAbilityInventorySlotBase::RefreshFrame()
{
    if (Frame)
    {
        Frame->SetStyleTag(DisplayState.bLocked ? LockedStyleTag : FrameStyleTag);
        SelectionMarker->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        PendingMarker->SetVisibility(bPending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UWidgetAbilityInventorySlotBase::SetPending(bool bInPending)
{
    bPending = bInPending;
    RefreshFrame();
}

void UWidgetAbilityInventorySlotBase::SetSelected(bool bInSelected)
{
    bSelected = bInSelected;
    RefreshFrame();
}

void UWidgetAbilityInventorySlotBase::SetCooldown(float InRemaining, float InDuration)
{
    const bool bValid = FMath::IsFinite(InRemaining) && FMath::IsFinite(InDuration) && InRemaining > 0.f && InDuration > 0.f;
    CooldownDuration = bValid ? InDuration : 0.f;
    CooldownEnd = FPlatformTime::Seconds() + (bValid ? InRemaining : 0.f);
    RestoreCooldownTimer();
}

void UWidgetAbilityInventorySlotBase::RestoreCooldownTimer()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CooldownTimer);
    AdvanceCooldown();
    if(GetWorld() && CooldownEnd > FPlatformTime::Seconds() && CooldownDuration > 0.f)
    {
        GetWorld()->GetTimerManager().SetTimer(CooldownTimer, this, &UWidgetAbilityInventorySlotBase::AdvanceCooldown, 0.05f, true);
    }
}

void UWidgetAbilityInventorySlotBase::NativeConstruct()
{
    Super::NativeConstruct();
    if (bHasDisplay) RefreshSlot(DisplayState, DisplayName, DisplayIcon.Get(), DisplayTooltip);
    RestoreCooldownTimer();
}

void UWidgetAbilityInventorySlotBase::AdvanceCooldown()
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

void UWidgetAbilityInventorySlotBase::NativeDestruct()
{
    if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CooldownTimer);
    SetToolTipText(FText::GetEmpty());
    SetToolTip(nullptr);
    Super::NativeDestruct();
}

FGuid UWidgetAbilityInventorySlotBase::GetInventoryId() const
{
    return InventoryId;
}

FInventorySlotKey UWidgetAbilityInventorySlotBase::GetSlotKey() const
{
    return Slot;
}

UWidgetAbilityInventoryBase* UWidgetAbilityInventorySlotBase::GetInventoryPanel() const
{
    return Panel.Get();
}

FReply UWidgetAbilityInventorySlotBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

FReply UWidgetAbilityInventorySlotBase::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (Panel.IsValid() && !bPending)
    {
        Panel->RequestUse(InventoryId, Slot);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UWidgetAbilityInventorySlotBase::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
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

bool UWidgetAbilityInventorySlotBase::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    const UInventoryDragOperation* Drag = Cast<UInventoryDragOperation>(InOperation);
    return Drag && Panel.IsValid() && !bPending && Panel->AcceptDrop(*Drag, InventoryId, Slot);
}

FReply UWidgetAbilityInventorySlotBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
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
