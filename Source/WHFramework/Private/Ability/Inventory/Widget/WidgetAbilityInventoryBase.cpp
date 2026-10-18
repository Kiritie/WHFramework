#include "Ability/Inventory/Widget/WidgetAbilityInventoryBase.h"

#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Ability/Inventory/Widget/WidgetInventorySlotView.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonRange.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonScroll.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"

UWidgetAbilityInventoryBase::UWidgetAbilityInventoryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SlotWidgetClass = UWidgetInventorySlotView::StaticClass();
    InventoryTitleStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Text);
    GroupTitleStyleTag = InventoryTitleStyleTag;
    SecondaryButtonStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Button);
    ConfirmButtonStyleTag = SecondaryButtonStyleTag;
}

TSharedRef<SWidget> UWidgetAbilityInventoryBase::RebuildWidget()
{
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }
    if (!WidgetTree->RootWidget)
    {
        UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryRoot"));
        ResultText = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("ResultText"));
        Root->AddChildToVerticalBox(ResultText);
        UHorizontalBox* Toolbar = WidgetTree->ConstructWidget<UHorizontalBox>();
        Btn_Sort = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_Sort"));
        Btn_Sort->SetTitle(NSLOCTEXT("WH.Inventory", "Sort", "整理当前分组"));
        Toolbar->AddChildToHorizontalBox(Btn_Sort);
        Btn_QuickStack = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_QuickStack"));
        Btn_QuickStack->SetTitle(NSLOCTEXT("WH.Inventory", "QuickStack", "快捷堆叠"));
        Toolbar->AddChildToHorizontalBox(Btn_QuickStack);
        Root->AddChildToVerticalBox(Toolbar);
        UCommonScroll* Scroll = WidgetTree->ConstructWidget<UCommonScroll>(UCommonScroll::StaticClass(), TEXT("InventoryScroll"));
        InventoryContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryContent"));
        Scroll->AddChild(InventoryContent);
        Root->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        SplitControls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SplitControls"));
        SplitCount = WidgetTree->ConstructWidget<UCommonRange>(UCommonRange::StaticClass(), TEXT("SplitCount"));
        SplitControls->AddChildToHorizontalBox(SplitCount)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Btn_ConfirmSplit = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_ConfirmSplit"));
        Btn_ConfirmSplit->SetTitle(NSLOCTEXT("WH.Inventory", "ConfirmSplit", "确认拆分"));
        SplitControls->AddChildToHorizontalBox(Btn_ConfirmSplit);
        Btn_CancelSplit = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_CancelSplit"));
        Btn_CancelSplit->SetTitle(NSLOCTEXT("WH.Inventory", "Cancel", "取消"));
        SplitControls->AddChildToHorizontalBox(Btn_CancelSplit);
        SplitControls->SetVisibility(ESlateVisibility::Collapsed);
        Root->AddChildToVerticalBox(SplitControls);
        WidgetTree->RootWidget = Root;
    }
    TSharedRef<SWidget> Result = Super::RebuildWidget();
    check(InventoryContent && ResultText && SplitControls && SplitCount && Btn_ConfirmSplit && Btn_CancelSplit);
    Btn_ConfirmSplit->SetInheritedStyleTag(ConfirmButtonStyleTag);
    Btn_CancelSplit->SetInheritedStyleTag(SecondaryButtonStyleTag);
    if (Btn_Sort)
    {
        Btn_Sort->SetInheritedStyleTag(SecondaryButtonStyleTag);
    }
    if (Btn_QuickStack)
    {
        Btn_QuickStack->SetInheritedStyleTag(SecondaryButtonStyleTag);
    }
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
    UnbindControls();
    UnbindInventories();
    Super::NativeDestruct();
}

void UWidgetAbilityInventoryBase::NativeConstruct()
{
    Super::NativeConstruct();
    BindControls();
}

void UWidgetAbilityInventoryBase::BindControls()
{
    UnbindControls();
    Btn_ConfirmSplit->OnClicked().AddUObject(this, &ThisClass::ConfirmSplit);
    Btn_CancelSplit->OnClicked().AddUObject(this, &ThisClass::CancelSplit);
    if (Btn_Sort)
    {
        Btn_Sort->OnClicked().AddUObject(this, &ThisClass::SortSelected);
    }
    if (Btn_QuickStack)
    {
        Btn_QuickStack->OnClicked().AddUObject(this, &ThisClass::QuickStackSelected);
    }
}

void UWidgetAbilityInventoryBase::UnbindControls()
{
    if (Btn_ConfirmSplit)
    {
        Btn_ConfirmSplit->OnClicked().RemoveAll(this);
    }
    if (Btn_CancelSplit)
    {
        Btn_CancelSplit->OnClicked().RemoveAll(this);
    }
    if (Btn_Sort)
    {
        Btn_Sort->OnClicked().RemoveAll(this);
    }
    if (Btn_QuickStack)
    {
        Btn_QuickStack->OnClicked().RemoveAll(this);
    }
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
    ClearViews();
    PendingSlots.Reset();
    SelectedInventoryId.Invalidate();
    SelectedSlot = FInventorySlotKey();
    CancelSplit();
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
    ClearViews();
    checkf(SlotHostPrefixes.IsEmpty() || InventoryIds.Num() <= 1, TEXT("Fixed inventory slot hosts require one bound inventory."));
    TMap<FGameplayTag, UPanelWidget*> GroupPanels;
    for (const TPair<FGameplayTag, FName>& Mapping : GroupPanelNames)
    {
        UPanelWidget* Host = Cast<UPanelWidget>(WidgetTree->FindWidget(Mapping.Value));
        checkf(Host, TEXT("Missing configured inventory group host %s"), *Mapping.Value.ToString());
        GroupPanels.Add(Mapping.Key, Host);
    }
    UPanelWidget* Inventories = nullptr;
    if (GroupPanelNames.IsEmpty() && SlotHostPrefixes.IsEmpty())
    {
        Inventories = bStackInventoriesVertically
            ? static_cast<UPanelWidget*>(WidgetTree->ConstructWidget<UVerticalBox>())
            : static_cast<UPanelWidget*>(WidgetTree->ConstructWidget<UHorizontalBox>());
        InventoryContent->AddChildToVerticalBox(Inventories);
    }
    for (const FGuid& InventoryId : InventoryIds)
    {
        const FInventorySnapshot* View = Network->FindView(InventoryId);
        if (!View)
        {
            continue;
        }
        UVerticalBox* Column = Inventories ? WidgetTree->ConstructWidget<UVerticalBox>() : nullptr;
        if (Inventories)
        {
            Inventories->AddChild(Column);
        }
        if (bShowInventoryTitles && Inventories)
        {
            UCommonTextBlockN* InventoryTitle = WidgetTree->ConstructWidget<UCommonTextBlockN>();
            InventoryTitle->SetStyleTag(InventoryTitleStyleTag);
            InventoryTitle->SetText(InventoryId == Network->GetOwnerInventoryId() ? NSLOCTEXT("WH.Inventory", "Owner", "背包") : NSLOCTEXT("WH.Inventory", "Container", "容器"));
            Column->AddChildToVerticalBox(InventoryTitle);
        }
        FGameplayTag LastGroup;
        UUniformGridPanel* Grid = nullptr;
        int32 Position = 0;
        for (const FInventorySlotState& SlotState : View->Slots)
        {
            if (!VisibleGroups.IsEmpty() && !VisibleGroups.Contains(SlotState.Key.Group))
            {
                continue;
            }
            UPanelWidget* FixedHost = nullptr;
            if (const FName* Prefix = SlotHostPrefixes.Find(SlotState.Key.Group))
            {
                const FName HostName(*FString::Printf(TEXT("%s%d"), *Prefix->ToString(), SlotState.Key.Index));
                FixedHost = Cast<UPanelWidget>(WidgetTree->FindWidget(HostName));
                checkf(FixedHost, TEXT("Missing configured inventory slot host %s"), *HostName.ToString());
                FixedHost->ClearChildren();
            }
            if (!FixedHost && (SlotState.Key.Group != LastGroup || !Grid))
            {
                LastGroup = SlotState.Key.Group;
                Position = 0;
                UPanelWidget* Destination = GroupPanels.FindRef(LastGroup);
                if (!Destination)
                {
                    Destination = Inventories ? static_cast<UPanelWidget*>(Column) : static_cast<UPanelWidget*>(InventoryContent);
                }
                if (bShowGroupTitles)
                {
                    UCommonTextBlockN* Title = WidgetTree->ConstructWidget<UCommonTextBlockN>();
                    Title->SetStyleTag(GroupTitleStyleTag);
                    Title->SetText(GroupNames.Contains(LastGroup) ? GroupNames.FindChecked(LastGroup) : FText::FromString(LastGroup.ToString()));
                    Destination->AddChild(Title);
                }
                Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
                Grid->SetSlotPadding(FMargin(2.f));
                Destination->AddChild(Grid);
            }
            UWidgetInventorySlotView* Widget = CreateWidget<UWidgetInventorySlotView>(GetOwningPlayer(), SlotWidgetClass);
            if(!Widget) continue;
            Widget->BindSlot(this, InventoryId, SlotState.Key);
            if (FixedHost)
            {
                FixedHost->AddChild(Widget);
            }
            else
            {
                const int32 GroupColumnCount = FMath::Max(1, GroupColumns.Contains(SlotState.Key.Group) ? GroupColumns.FindChecked(SlotState.Key.Group) : Columns);
                Grid->AddChildToUniformGrid(Widget, Position / GroupColumnCount, Position % GroupColumnCount);
                ++Position;
            }
            Slots.Add(SlotId(InventoryId, SlotState.Key), Widget);
            RefreshSlot(InventoryId, SlotState);
        }
    }
    RefreshPending();
}

void UWidgetAbilityInventoryBase::ClearViews()
{
    for (const TPair<FString, TWeakObjectPtr<UWidgetInventorySlotView>>& Entry : Slots)
    {
        if (Entry.Value.IsValid())
        {
            Entry.Value->RemoveFromParent();
        }
    }
    Slots.Reset();
    if (InventoryContent)
    {
        InventoryContent->ClearChildren();
    }
    if (WidgetTree)
    {
        for (const TPair<FGameplayTag, FName>& Mapping : GroupPanelNames)
        {
            UPanelWidget* Host = Cast<UPanelWidget>(WidgetTree->FindWidget(Mapping.Value));
            if (Host && Host != InventoryContent)
            {
                Host->ClearChildren();
            }
        }
    }
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
    if (!Source || !Target || Source->bLocked || Target->bLocked || InDrag.Quantity <= 0 ||
        Source->Item.Signature() != InDrag.Signature || Source->Item.Count < InDrag.Quantity ||
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
        if (!Target->Item.IsEmpty() || Source->Item.Count <= 1 || Source->Item.InstanceId.IsValid() || !SplitControls || !SplitCount)
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
        FCommonRangeConfig Config;
        Config.DecimalPlaces = 0;
        Config.bHasMin = true;
        Config.bHasMax = true;
        Config.Min = 1;
        Config.Max = Source->Item.Count - 1;
        Config.Step = 1;
        Config.bShowStepButtons = true;
        SplitCount->Configure(Config);
        SplitCount->SetValue(FParameter(Source->Item.Count / 2));
        SplitControls->SetVisibility(ESlateVisibility::Visible);
        SplitCount->SetInputFocus();
        return true;
    }
    return Submit(Request);
}

void UWidgetAbilityInventoryBase::ConfirmSplit()
{
    if (SplitControls && SplitControls->GetVisibility() == ESlateVisibility::Visible)
    {
        FInventoryClientRequest Request = SplitRequest;
        Request.Quantity = SplitCount->GetValue().Get<int32>();
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
