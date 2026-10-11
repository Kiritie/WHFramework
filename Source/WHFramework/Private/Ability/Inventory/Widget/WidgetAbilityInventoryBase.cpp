#include "Ability/Inventory/Widget/WidgetAbilityInventoryBase.h"

#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Ability/Inventory/Widget/WidgetInventorySlotView.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"

UWidgetAbilityInventoryBase::UWidgetAbilityInventoryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SlotWidgetClass = UWidgetInventorySlotView::StaticClass();
}

TSharedRef<SWidget> UWidgetAbilityInventoryBase::RebuildWidget()
{
    if (WidgetTree && WidgetTree->RootWidget && InventoryContent)
    {
        TSharedRef<SWidget> Result = Super::RebuildWidget();
        RebuildViews();
        return Result;
    }
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryRoot"));
    ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryResult"));
    Root->AddChildToVerticalBox(ResultText);
    UHorizontalBox* Toolbar = WidgetTree->ConstructWidget<UHorizontalBox>();
    UButton* Sort = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* SortLabel = WidgetTree->ConstructWidget<UTextBlock>();
    SortLabel->SetText(NSLOCTEXT("WH.Inventory", "Sort", "整理当前分组"));
    Sort->SetContent(SortLabel);
    Sort->OnClicked.AddDynamic(this, &UWidgetAbilityInventoryBase::SortSelected);
    Toolbar->AddChildToHorizontalBox(Sort);
    UButton* Quick = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* QuickLabel = WidgetTree->ConstructWidget<UTextBlock>();
    QuickLabel->SetText(NSLOCTEXT("WH.Inventory", "QuickStack", "快捷堆叠"));
    Quick->SetContent(QuickLabel);
    Quick->OnClicked.AddDynamic(this, &UWidgetAbilityInventoryBase::QuickStackSelected);
    Toolbar->AddChildToHorizontalBox(Quick);
    Root->AddChildToVerticalBox(Toolbar);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("InventoryScroll"));
    InventoryContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryContent"));
    Scroll->AddChild(InventoryContent);
    Root->AddChildToVerticalBox(Scroll);
    SplitControls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SplitControls"));
    SplitCount = WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), TEXT("SplitCount"));
    SplitCount->SetDelta(1.f);
    SplitControls->AddChildToHorizontalBox(SplitCount);
    UButton* Confirm = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmSplit"));
    UTextBlock* ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmSplitText"));
    ConfirmText->SetText(NSLOCTEXT("WH.Inventory", "ConfirmSplit", "确认拆分"));
    Confirm->SetContent(ConfirmText);
    Confirm->OnClicked.AddDynamic(this, &UWidgetAbilityInventoryBase::ConfirmSplit);
    SplitControls->AddChildToHorizontalBox(Confirm);
    UButton* Cancel = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CancelSplit"));
    UTextBlock* CancelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CancelSplitText"));
    CancelText->SetText(NSLOCTEXT("WH.Inventory", "Cancel", "取消"));
    Cancel->SetContent(CancelText);
    Cancel->OnClicked.AddDynamic(this, &UWidgetAbilityInventoryBase::CancelSplit);
    SplitControls->AddChildToHorizontalBox(Cancel);
    SplitControls->SetVisibility(ESlateVisibility::Collapsed);
    Root->AddChildToVerticalBox(SplitControls);
    WidgetTree->RootWidget = Root;
    TSharedRef<SWidget> Result = Super::RebuildWidget();
    RebuildViews();
    return Result;
}

void UWidgetAbilityInventoryBase::OnInitialize(const FParameter& InParams)
{
    Super::OnInitialize(InParams);
}

void UWidgetAbilityInventoryBase::OnRefresh()
{
    Super::OnRefresh();
}

void UWidgetAbilityInventoryBase::OnClose(bool bInstant)
{
    UnbindInventories();
    Super::OnClose(bInstant);
}

void UWidgetAbilityInventoryBase::NativeDestruct()
{
    UnbindInventories();
    Super::NativeDestruct();
}

void UWidgetAbilityInventoryBase::BindInventories(UInventoryNetworkComponent* InNetwork, const TArray<FGuid>& InInventoryIds, bool bInFollowOwner)
{
    UnbindInventories();
    if (!IsValid(InNetwork))
    {
        return;
    }
    Network = InNetwork;
    bFollowOwner = bInFollowOwner;
    FollowedOwnerId = bFollowOwner ? InNetwork->GetOwnerInventoryId() : FGuid();
    for (const FGuid& InventoryId : InInventoryIds)
    {
        if (InventoryId.IsValid())
        {
            InventoryIds.AddUnique(InventoryId);
        }
    }
    if (FollowedOwnerId.IsValid())
    {
        InventoryIds.AddUnique(FollowedOwnerId);
    }
    SnapshotHandle = InNetwork->OnSnapshot.AddUObject(this, &UWidgetAbilityInventoryBase::HandleSnapshot);
    DeltaHandle = InNetwork->OnDelta.AddUObject(this, &UWidgetAbilityInventoryBase::HandleDelta);
    ResultHandle = InNetwork->OnResult.AddUObject(this, &UWidgetAbilityInventoryBase::HandleResult);
    ClosedHandle = InNetwork->OnSessionClosed.AddUObject(this, &UWidgetAbilityInventoryBase::HandleClosed);
    for (const FGuid& InventoryId : InventoryIds)
    {
        if (!InNetwork->FindView(InventoryId))
        {
            InNetwork->OpenInventory(InventoryId);
        }
    }
    RebuildViews();
}

void UWidgetAbilityInventoryBase::UnbindInventories()
{
    if (Network.IsValid())
    {
        Network->OnSnapshot.Remove(SnapshotHandle);
        Network->OnDelta.Remove(DeltaHandle);
        Network->OnResult.Remove(ResultHandle);
        Network->OnSessionClosed.Remove(ClosedHandle);
        for (const FGuid& InventoryId : InventoryIds)
        {
            if (InventoryId != Network->GetOwnerInventoryId())
            {
                Network->CloseInventory(InventoryId);
            }
        }
    }
    Network.Reset();
    bFollowOwner = false;
    FollowedOwnerId.Invalidate();
    InventoryIds.Reset();
    Slots.Reset();
    PendingSlots.Reset();
    SelectedInventoryId.Invalidate();
    SelectedSlot = FInventorySlotKey();
    CancelSplit();
    if (InventoryContent)
    {
        InventoryContent->ClearChildren();
    }
    SetToolTipText(FText::GetEmpty());
}

FString UWidgetAbilityInventoryBase::SlotId(const FGuid& InInventoryId, const FInventorySlotKey& InSlot)
{
    return InInventoryId.ToString(EGuidFormats::Digits) + TEXT("/") + InSlot.Group.ToString() + TEXT("/") + LexToString(InSlot.Index);
}

void UWidgetAbilityInventoryBase::RebuildViews()
{
    if (!InventoryContent || !Network.IsValid())
    {
        return;
    }
    InventoryContent->ClearChildren();
    Slots.Reset();
    UHorizontalBox* ColumnsRoot = WidgetTree->ConstructWidget<UHorizontalBox>();
    InventoryContent->AddChildToVerticalBox(ColumnsRoot);
    for (const FGuid& InventoryId : InventoryIds)
    {
        const FInventorySnapshot* View = Network->FindView(InventoryId);
        if (!View)
        {
            continue;
        }
        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
        ColumnsRoot->AddChildToHorizontalBox(Column);
        UTextBlock* InventoryTitle = WidgetTree->ConstructWidget<UTextBlock>();
        InventoryTitle->SetText(InventoryId == Network->GetOwnerInventoryId() ? NSLOCTEXT("WH.Inventory", "Owner", "背包") : NSLOCTEXT("WH.Inventory", "Container", "容器"));
        Column->AddChildToVerticalBox(InventoryTitle);
        FGameplayTag LastGroup;
        UUniformGridPanel* Grid = nullptr;
        int32 Position = 0;
        for (const FInventorySlotState& SlotState : View->Slots)
        {
            if (!VisibleGroups.IsEmpty() && !VisibleGroups.Contains(SlotState.Key.Group))
            {
                continue;
            }
            if (SlotState.Key.Group != LastGroup || !Grid)
            {
                LastGroup = SlotState.Key.Group;
                Position = 0;
                UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
                Title->SetText(GroupNames.Contains(LastGroup) ? GroupNames.FindChecked(LastGroup) : FText::FromString(LastGroup.ToString()));
                Column->AddChildToVerticalBox(Title);
                Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
                Grid->SetSlotPadding(FMargin(2.f));
                Column->AddChildToVerticalBox(Grid);
            }
            UWidgetInventorySlotView* Widget = CreateWidget<UWidgetInventorySlotView>(GetOwningPlayer(), SlotWidgetClass);
            if(!Widget) continue;
            Widget->BindSlot(this, InventoryId, SlotState.Key);
            Grid->AddChildToUniformGrid(Widget, Position / FMath::Max(1, Columns), Position % FMath::Max(1, Columns));
            ++Position;
            Slots.Add(SlotId(InventoryId, SlotState.Key), Widget);
            RefreshSlot(InventoryId, SlotState);
        }
    }
    RefreshPending();
}

void UWidgetAbilityInventoryBase::RefreshSlot(const FGuid& InInventoryId, const FInventorySlotState& InSlot)
{
    const TWeakObjectPtr<UWidgetInventorySlotView>* Found = Slots.Find(SlotId(InInventoryId, InSlot.Key));
    if (!Found || !Found->IsValid())
    {
        return;
    }
    UInventoryWorldSubsystem* System = GetWorld()->GetSubsystem<UInventoryWorldSubsystem>();
    const UInventoryItemAssetCatalog* Assets = System ? System->GetItemAssets() : nullptr;
    const UAbilityItemDataBase* Definition = Assets ? Assets->Find(InSlot.Item.ItemId) : nullptr;
    const FText Name = Definition ? Definition->Name : FText::FromString(InSlot.Item.ItemId.ToString());
    FText Tooltip = Definition ? FText::Format(NSLOCTEXT("WH.Inventory", "Tooltip", "{0}\n等级 {1}\n{2}"), Name, FText::AsNumber(InSlot.Item.Level), Definition->Detail) : Name;
    Found->Get()->RefreshSlot(InSlot, Name, Definition ? Definition->Icon : nullptr, Tooltip);
    Found->Get()->SetSelected(InInventoryId == SelectedInventoryId && InSlot.Key == SelectedSlot);
}

void UWidgetAbilityInventoryBase::HandleSnapshot(const FInventorySnapshot& InSnapshot)
{
    if (bFollowOwner && Network.IsValid() && InSnapshot.InventoryId == Network->GetOwnerInventoryId())
    {
        if (FollowedOwnerId.IsValid() && FollowedOwnerId != InSnapshot.InventoryId)
        {
            InventoryIds.Remove(FollowedOwnerId);
        }
        FollowedOwnerId = InSnapshot.InventoryId;
        InventoryIds.AddUnique(FollowedOwnerId);
        const TArray<FGuid> RequestedInventories = InventoryIds;
        for (const FGuid& InventoryId : RequestedInventories)
        {
            if (InventoryId != FollowedOwnerId && !Network->FindView(InventoryId))
            {
                Network->OpenInventory(InventoryId);
            }
        }
    }
    if (InventoryIds.Contains(InSnapshot.InventoryId))
    {
        RebuildViews();
    }
}

void UWidgetAbilityInventoryBase::HandleDelta(const FInventoryDelta& InDelta)
{
    if (InventoryIds.Contains(InDelta.InventoryId))
    {
        for (const FInventorySlotState& State : InDelta.Slots)
        {
            RefreshSlot(InDelta.InventoryId, State);
        }
    }
}

void UWidgetAbilityInventoryBase::HandleResult(const FInventoryCommandResult& InResult)
{
    if (PendingSlots.Remove(InResult.RequestId) == 0)
    {
        return;
    }
    RefreshPending();
    if (ResultText)
    {
        if(InResult.Code == EInventoryResultCode::ConfirmationPending)
        {
            ResultText->SetText(NSLOCTEXT("WH.Inventory", "AwaitConfirmation", "结果待确认，请等待重新同步库存。"));
            return;
        }
        if (InResult.IsSuccess())
        {
            ResultText->SetText(InResult.RemainingCount > 0 ? FText::Format(NSLOCTEXT("WH.Inventory", "Partial", "已处理 {0}，剩余 {1}"), FText::AsNumber(InResult.AcceptedCount), FText::AsNumber(InResult.RemainingCount)) : FText::GetEmpty());
        }
        else
        {
            const UEnum* Codes = StaticEnum<EInventoryResultCode>();
            ResultText->SetText(FText::Format(NSLOCTEXT("WH.Inventory", "Rejected", "操作未完成：{0}"), Codes->GetDisplayNameTextByValue(static_cast<int64>(InResult.Code))));
        }
    }
}

void UWidgetAbilityInventoryBase::HandleClosed(const FGuid& InInventoryId, EInventoryResultCode InReason)
{
    if (InventoryIds.Remove(InInventoryId) > 0)
    {
        CancelSplit();
        if (SelectedInventoryId == InInventoryId)
        {
            SelectedInventoryId.Invalidate();
            SelectedSlot = FInventorySlotKey();
        }
        RebuildViews();
        if (ResultText)
        {
            ResultText->SetText(NSLOCTEXT("WH.Inventory", "Closed", "容器会话已关闭"));
        }
    }
}

const FInventorySlotState* UWidgetAbilityInventoryBase::FindSlot(const FGuid& InInventoryId, const FInventorySlotKey& InSlot) const
{
    const FInventorySnapshot* View = Network.IsValid() ? Network->FindView(InInventoryId) : nullptr;
    return View ? View->FindSlot(InSlot) : nullptr;
}

UInventoryNetworkComponent* UWidgetAbilityInventoryBase::GetNetwork() const
{
    return Network.Get();
}

FGuid UWidgetAbilityInventoryBase::GetSelectedInventoryId() const
{
    return SelectedInventoryId;
}

FInventorySlotKey UWidgetAbilityInventoryBase::GetSelectedSlotKey() const
{
    return SelectedSlot;
}

void UWidgetAbilityInventoryBase::SelectSlot(FGuid InInventoryId, FInventorySlotKey InSlot)
{
    if (!FindSlot(InInventoryId, InSlot))
    {
        return;
    }
    const TWeakObjectPtr<UWidgetInventorySlotView>* Previous = Slots.Find(SlotId(SelectedInventoryId, SelectedSlot));
    if (Previous && Previous->IsValid())
    {
        Previous->Get()->SetSelected(false);
    }
    SelectedInventoryId = InInventoryId;
    SelectedSlot = InSlot;
    const TWeakObjectPtr<UWidgetInventorySlotView>* Current = Slots.Find(SlotId(InInventoryId, InSlot));
    if (Current && Current->IsValid())
    {
        Current->Get()->SetSelected(true);
    }
    OnSelection.Broadcast(InInventoryId, InSlot);
}

bool UWidgetAbilityInventoryBase::Submit(FInventoryClientRequest InRequest)
{
    if (!Network.IsValid())
    {
        return false;
    }
    TArray<FString> Affected;
    if (InRequest.SourceSlot.IsValid())
    {
        Affected.Add(SlotId(InRequest.SourceInventoryId, InRequest.SourceSlot));
    }
    if (InRequest.TargetSlot.IsValid())
    {
        Affected.AddUnique(SlotId(InRequest.TargetInventoryId, InRequest.TargetSlot));
    }
    const auto AddInventorySlots = [&](const FGuid& InventoryId, const FGameplayTag& Group)
    {
        const FInventorySnapshot* View = Network->FindView(InventoryId);
        if (!View)
        {
            return;
        }
        for (const FInventorySlotState& Slot : View->Slots)
        {
            if (!Group.IsValid() || Slot.Key.Group == Group)
            {
                Affected.AddUnique(SlotId(InventoryId, Slot.Key));
            }
        }
    };
    if (!InRequest.SourceSlot.IsValid())
    {
        AddInventorySlots(InRequest.SourceInventoryId, InRequest.SourceGroup);
    }
    if (InRequest.TargetInventoryId.IsValid() && !InRequest.TargetSlot.IsValid())
    {
        AddInventorySlots(InRequest.TargetInventoryId, InRequest.TargetGroup);
    }
    for (const FGuid& InventoryId : InRequest.TargetInventories)
    {
        AddInventorySlots(InventoryId, InRequest.TargetGroup);
    }
    for (const TPair<uint64, TArray<FString>>& Pair : PendingSlots)
    {
        for (const FString& Key : Affected)
        {
            if (Pair.Value.Contains(Key))
            {
                return false;
            }
        }
    }
    const uint64 RequestId = Network->SubmitRequest(InRequest);
    if (RequestId == 0)
    {
        return false;
    }
    PendingSlots.Add(RequestId, MoveTemp(Affected));
    RefreshPending();
    return true;
}

void UWidgetAbilityInventoryBase::RefreshPending()
{
    TSet<FString> PendingKeys;
    for (const TPair<uint64, TArray<FString>>& Pair : PendingSlots)
    {
        for (const FString& Key : Pair.Value)
        {
            PendingKeys.Add(Key);
        }
    }
    for (const TPair<FString, TWeakObjectPtr<UWidgetInventorySlotView>>& Pair : Slots)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->SetPending(PendingKeys.Contains(Pair.Key));
        }
    }
}

bool UWidgetAbilityInventoryBase::RequestUse(FGuid InInventoryId, FInventorySlotKey InSlot)
{
    if(UseAction.IsNone()) return false;
    const FInventorySlotState* State = FindSlot(InInventoryId, InSlot);
    if (!State || State->Item.IsEmpty())
    {
        return false;
    }
    FInventoryClientRequest Request;
    Request.DomainAction = UseAction;
    Request.SourceInventoryId = InInventoryId;
    Request.SourceSlot = InSlot;
    return Submit(Request);
}

bool UWidgetAbilityInventoryBase::RequestDrop(FGuid InInventoryId, FInventorySlotKey InSlot, int32 InQuantity)
{
    if(DropAction.IsNone()) return false;
    FInventoryClientRequest Request;
    Request.DomainAction = DropAction;
    Request.SourceInventoryId = InInventoryId;
    Request.SourceSlot = InSlot;
    Request.Quantity = InQuantity;
    return Submit(Request);
}

bool UWidgetAbilityInventoryBase::RequestTransfer(FGuid InInventoryId, FInventorySlotKey InSlot, int32 InQuantity)
{
    const FGuid* Target = InventoryIds.FindByPredicate([&](const FGuid& Candidate) { return Candidate != InInventoryId; });
    if (!Target)
    {
        return false;
    }
    FInventoryClientRequest Request;
    Request.Operation = EInventoryOperationType::Transfer;
    Request.SourceInventoryId = InInventoryId;
    Request.TargetInventoryId = *Target;
    Request.SourceSlot = InSlot;
    Request.Quantity = InQuantity;
    Request.bAllowPartial = true;
    return Submit(Request);
}

bool UWidgetAbilityInventoryBase::RequestSort(FGuid InInventoryId, FGameplayTag InGroup)
{
    FInventoryClientRequest Request;
    Request.Operation = EInventoryOperationType::Sort;
    Request.SourceInventoryId = InInventoryId;
    Request.SourceGroup = InGroup;
    return Submit(Request);
}

bool UWidgetAbilityInventoryBase::RequestFavorite(FGuid InInventoryId, FInventorySlotKey InSlot)
{
    const FInventorySlotState* State = FindSlot(InInventoryId, InSlot);
    if (!State || State->Item.IsEmpty())
    {
        return false;
    }
    FInventoryClientRequest Request;
    Request.Operation = EInventoryOperationType::SetFavorite;
    Request.SourceInventoryId = InInventoryId;
    Request.SourceSlot = InSlot;
    Request.bFavorite = !State->bFavorite;
    return Submit(Request);
}

bool UWidgetAbilityInventoryBase::AcceptDrop(const UInventoryDragOperation& InDrag, const FGuid& InTargetInventoryId, const FInventorySlotKey& InTargetSlot)
{
    if (!Network.IsValid() || !InDrag.WorldEpoch.IsValid() || InDrag.WorldEpoch != Network->GetWorldEpoch() ||
        !InDrag.SourceSessionId.IsValid() || InDrag.SourceSessionId != Network->GetSessionId(InDrag.InventoryId) ||
        !Network->GetSessionId(InTargetInventoryId).IsValid())
    {
        return false;
    }
    const FInventorySlotState* Source = FindSlot(InDrag.InventoryId, InDrag.Slot);
    const FInventorySlotState* Target = FindSlot(InTargetInventoryId, InTargetSlot);
    if (!Source || !Target || Source->Item.Signature() != InDrag.Signature || Source->Item.Count < InDrag.Quantity ||
        (InDrag.InventoryId == InTargetInventoryId && InDrag.Slot == InTargetSlot))
    {
        return false;
    }
    FInventoryClientRequest Request;
    Request.Operation = EInventoryOperationType::Move;
    Request.SourceInventoryId = InDrag.InventoryId;
    Request.TargetInventoryId = InTargetInventoryId;
    Request.SourceSlot = InDrag.Slot;
    Request.TargetSlot = InTargetSlot;
    Request.Quantity = InDrag.Quantity;
    Request.bAllowPartial = true;
    if (!Target->Item.IsEmpty() && !Source->Item.CanStackWith(Target->Item))
    {
        if (InDrag.bChooseQuantity || Request.Quantity != Source->Item.Count)
        {
            return false;
        }
        Request.Operation = EInventoryOperationType::Swap;
        Request.bAllowPartial = false;
    }
    if (InDrag.bChooseQuantity)
    {
        if (!Target->Item.IsEmpty() || !SplitControls || !SplitCount)
        {
            return false;
        }
        SplitRequest = Request;
        SplitSignature = Source->Item.Signature();
        SplitWorldEpoch = Network->GetWorldEpoch();
        SplitSourceSessionId = Network->GetSessionId(InDrag.InventoryId);
        SplitTargetSessionId = Network->GetSessionId(InTargetInventoryId);
        SplitRequest.Operation = EInventoryOperationType::Split;
        SplitRequest.bAllowPartial = false;
        SplitCount->SetMinValue(1.f);
        SplitCount->SetMaxValue(static_cast<float>(Source->Item.Count - 1));
        SplitCount->SetMinSliderValue(1.f);
        SplitCount->SetMaxSliderValue(static_cast<float>(Source->Item.Count - 1));
        SplitCount->SetValue(static_cast<float>(Source->Item.Count / 2));
        SplitControls->SetVisibility(ESlateVisibility::Visible);
        SplitCount->SetKeyboardFocus();
        return true;
    }
    return Submit(Request);
}

void UWidgetAbilityInventoryBase::ConfirmSplit()
{
    if (SplitControls && SplitControls->GetVisibility() == ESlateVisibility::Visible)
    {
        FInventoryClientRequest Request = SplitRequest;
        Request.Quantity = FMath::FloorToInt(SplitCount->GetValue());
        const FInventorySlotState* Source = FindSlot(Request.SourceInventoryId, Request.SourceSlot);
        const FInventorySlotState* Target = FindSlot(Request.TargetInventoryId, Request.TargetSlot);
        const bool bMatches = Network.IsValid() && SplitWorldEpoch == Network->GetWorldEpoch() &&
            SplitSourceSessionId.IsValid() && SplitSourceSessionId == Network->GetSessionId(Request.SourceInventoryId) &&
            SplitTargetSessionId.IsValid() && SplitTargetSessionId == Network->GetSessionId(Request.TargetInventoryId) &&
            Source && Target && Source->Item.Signature() == SplitSignature &&
            Request.Quantity > 0 && Request.Quantity < Source->Item.Count && Target->Item.IsEmpty();
        CancelSplit();
        if (bMatches)
        {
            Submit(Request);
        }
        else if (ResultText)
        {
            ResultText->SetText(NSLOCTEXT("WH.Inventory", "SplitChanged", "拆分期间物品已变化，请重新选择。"));
        }
    }
}

void UWidgetAbilityInventoryBase::CancelSplit()
{
    SplitRequest = FInventoryClientRequest();
    SplitSignature = FInventoryStackSignature();
    SplitWorldEpoch.Invalidate();
    SplitSourceSessionId.Invalidate();
    SplitTargetSessionId.Invalidate();
    if (SplitControls)
    {
        SplitControls->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UWidgetAbilityInventoryBase::Navigate(const FGuid& InInventoryId, const FInventorySlotKey& InSlot, int32 InOffset)
{
    FInventorySlotKey Target = InSlot;
    Target.Index += InOffset == -10 ? -Columns : (InOffset == 10 ? Columns : InOffset);
    const TWeakObjectPtr<UWidgetInventorySlotView>* Widget = Slots.Find(SlotId(InInventoryId, Target));
    if (Widget && Widget->IsValid())
    {
        SelectSlot(InInventoryId, Target);
        Widget->Get()->SetKeyboardFocus();
    }
}

void UWidgetAbilityInventoryBase::UpdateCooldown(const FGuid& InInventoryId, const FInventorySlotKey& InSlot, float InRemaining, float InDuration)
{
    const TWeakObjectPtr<UWidgetInventorySlotView>* Widget = Slots.Find(SlotId(InInventoryId, InSlot));
    if (Widget && Widget->IsValid())
    {
        Widget->Get()->SetCooldown(InRemaining, InDuration);
    }
}

bool UWidgetAbilityInventoryBase::RequestQuickStack(FGuid InInventoryId)
{
    FInventoryClientRequest Request;
    Request.Operation = EInventoryOperationType::QuickStack;
    Request.SourceInventoryId = InInventoryId;
    Request.bAutomatic = true;
    for(const FGuid& Target : InventoryIds)
    {
        if(Target != InInventoryId) Request.TargetInventories.Add(Target);
    }
    return !Request.TargetInventories.IsEmpty() && Submit(MoveTemp(Request));
}

void UWidgetAbilityInventoryBase::SortSelected()
{
    if(SelectedInventoryId.IsValid() && SelectedSlot.IsValid()) RequestSort(SelectedInventoryId, SelectedSlot.Group);
}

void UWidgetAbilityInventoryBase::QuickStackSelected()
{
    if(Network.IsValid()) RequestQuickStack(Network->GetOwnerInventoryId());
}
