#include "Ability/Inventory/Widget/WidgetAbilityInventoryBase.h"

#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Ability/Inventory/Widget/WidgetAbilityInventorySlotBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Blueprint/WidgetTree.h"
#include "TimerManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
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
    SlotWidgetClass = UWidgetAbilityInventorySlotBase::StaticClass();
    InventoryTitleStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Text);
    GroupTitleStyleTag = InventoryTitleStyleTag;
    SecondaryButtonStyleTag = WidgetStyleTags::CommonDefault(EWidgetStyleKind::Button);
}

TSharedRef<SWidget> UWidgetAbilityInventoryBase::RebuildWidget()
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
        UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryRoot"));
        ResultText = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("ResultText"));
        Root->AddChildToVerticalBox(ResultText);
        InventoryActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryActions"));
        Btn_Sort = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_Sort"));
        Btn_Sort->SetTitle(NSLOCTEXT("WH.Inventory", "Sort", "整理当前分组"));
        InventoryActions->AddChildToHorizontalBox(Btn_Sort);
        Btn_QuickStack = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_QuickStack"));
        Btn_QuickStack->SetTitle(NSLOCTEXT("WH.Inventory", "QuickStack", "快捷堆叠"));
        InventoryActions->AddChildToHorizontalBox(Btn_QuickStack);
        Root->AddChildToVerticalBox(InventoryActions);
        UCommonScroll* Scroll = WidgetTree->ConstructWidget<UCommonScroll>(UCommonScroll::StaticClass(), TEXT("InventoryScroll"));
        InventoryContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryContent"));
        Scroll->AddChild(InventoryContent);
        Root->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        WidgetTree->RootWidget = Root;
    }
    TSharedRef<SWidget> Result = Super::RebuildWidget();
    check(InventoryContent && ResultText);
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

void UWidgetAbilityInventoryBase::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    SetShowExtensionButtons(bShowExtensionButtons);
}

void UWidgetAbilityInventoryBase::SetShowExtensionButtons(bool bInShowExtensionButtons)
{
    bShowExtensionButtons = bInShowExtensionButtons;
    const ESlateVisibility ActionsVisibility = bShowExtensionButtons ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
    if (InventoryActions)
    {
        InventoryActions->SetVisibility(ActionsVisibility);
    }
    else
    {
        for (UCommonButton* Button : {Btn_Sort.Get(), Btn_QuickStack.Get()})
        {
            if (Button)
            {
                Button->SetVisibility(bShowExtensionButtons ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            }
        }
    }
}

void UWidgetAbilityInventoryBase::OnRefresh()
{
    Super::OnRefresh();
}

void UWidgetAbilityInventoryBase::OnReset(bool bForce)
{
    UnbindInventories();
    Super::OnReset(bForce);
}

void UWidgetAbilityInventoryBase::NativeDestruct()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ResultFadeTimer);
    CancelSplit();
    UnbindControls();

    Super::NativeDestruct();
}

void UWidgetAbilityInventoryBase::OnDestroy(EObjectDespawnMode InMode)
{
    UnbindControls();
    UnbindInventories();
    Super::OnDestroy(InMode);
}

void UWidgetAbilityInventoryBase::NativeConstruct()
{
    Super::NativeConstruct();
    BindControls();
    ResumeResultFade();
}

void UWidgetAbilityInventoryBase::BindControls()
{
    UnbindControls();
    if (Btn_Sort)
    {
        Btn_Sort->OnClicked().AddUObject(this, &ThisClass::SortSelected);
    }
    if (Btn_QuickStack)
    {
        Btn_QuickStack->OnClicked().AddUObject(this, &ThisClass::QuickStackSelected);
    }
    RefreshControls();
}

void UWidgetAbilityInventoryBase::RefreshControls()
{
    if (Btn_Sort)
    {
        Btn_Sort->SetIsEnabled(FindSlot(SelectedInventoryId, SelectedSlot) != nullptr);
    }
    if (Btn_QuickStack)
    {
        Btn_QuickStack->SetIsEnabled(Network.IsValid() && Network->FindView(Network->GetOwnerInventoryId()) != nullptr);
    }
}

void UWidgetAbilityInventoryBase::UnbindControls()
{
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
    if (!IsValid(InNetwork) || InNetwork->GetWorld() != GetWorld())
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
    QueuedHandle = InNetwork->OnRequestQueued.AddUObject(this, &UWidgetAbilityInventoryBase::HandleRequestQueued);
    ClosedHandle = InNetwork->OnSessionClosed.AddUObject(this, &UWidgetAbilityInventoryBase::HandleClosed);
    const TArray<FGuid> RequestedInventories = InventoryIds;
    for (const FGuid& InventoryId : RequestedInventories)
    {
        if (Network.Get() != InNetwork) return;
        if (!InNetwork->FindView(InventoryId))
        {
            InNetwork->OpenInventory(InventoryId);
        }
    }
    if (Network.Get() != InNetwork) return;
    for (const FInventoryClientRequest& Request : InNetwork->GetPendingRequests())
    {
        if (Network.Get() != InNetwork) return;
        HandleRequestQueued(Request);
    }
    if (Network.Get() != InNetwork) return;
    RebuildViews();
}

void UWidgetAbilityInventoryBase::UnbindInventories()
{
    if (Network.IsValid())
    {
        Network->OnSnapshot.Remove(SnapshotHandle);
        Network->OnDelta.Remove(DeltaHandle);
        Network->OnResult.Remove(ResultHandle);
        Network->OnRequestQueued.Remove(QueuedHandle);
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
    TransferInventoryId.Invalidate();
    ClearViews();
    PendingSlots.Reset();
    SelectedInventoryId.Invalidate();
    SelectedSlot = FInventorySlotKey();
    CancelSplit();
    SetToolTipText(FText::GetEmpty());
    SetResultText(FText::GetEmpty());
}

FString UWidgetAbilityInventoryBase::SlotId(const FGuid& InInventoryId, const FInventorySlotKey& InSlot)
{
    return InInventoryId.ToString(EGuidFormats::Digits) + TEXT("/") + InSlot.Group.ToString() + TEXT("/") + LexToString(InSlot.Index);
}

void UWidgetAbilityInventoryBase::SetGroupSlotHosts(FGameplayTag InGroup, const TMap<int32, UPanelWidget*>& InHosts)
{
    check(InGroup.IsValid());
    TMap<int32, TWeakObjectPtr<UPanelWidget>> Hosts;
    for (const TPair<int32, UPanelWidget*>& Entry : InHosts)
    {
        check(Entry.Key >= 0 && IsValid(Entry.Value) && Entry.Value->GetWorld() == GetWorld());
        Hosts.Add(Entry.Key, Entry.Value);
    }
    const TMap<int32, TWeakObjectPtr<UPanelWidget>>* Previous = GroupSlotHosts.Find(InGroup);
    if (Previous && Previous->OrderIndependentCompareEqual(Hosts))
    {
        return;
    }
    GroupSlotHosts.Add(InGroup, MoveTemp(Hosts));
    RebuildViews();
}

void UWidgetAbilityInventoryBase::RebuildViews()
{
    if (!InventoryContent)
    {
        return;
    }
    const uint64 Revision = ClearViews();
    const UInventoryNetworkComponent* SourceNetwork = Network.Get();
    if (!SourceNetwork || Revision != ViewRevision)
    {
        return;
    }
    const bool bFixedSize = !GridSlotSize.IsNearlyZero();
    checkf(!bFixedSize || (FMath::IsFinite(GridSlotSize.X) && FMath::IsFinite(GridSlotSize.Y) && GridSlotSize.X > 0.f && GridSlotSize.Y > 0.f), TEXT("Fixed inventory cells require a positive finite size."));
    checkf(SlotHostPrefixes.IsEmpty() || InventoryIds.Num() <= 1, TEXT("Fixed inventory slot hosts require one bound inventory."));
    checkf(GroupSlotHosts.IsEmpty() || InventoryIds.Num() <= 1, TEXT("External inventory slot hosts require one bound inventory."));
    TMap<FGameplayTag, UPanelWidget*> GroupPanels;
    for (const TPair<FGameplayTag, FName>& Mapping : GroupPanelNames)
    {
        UPanelWidget* Host = Cast<UPanelWidget>(WidgetTree->FindWidget(Mapping.Value));
        checkf(Host, TEXT("Missing configured inventory group host %s"), *Mapping.Value.ToString());
        GroupPanels.Add(Mapping.Key, Host);
    }
    UPanelWidget* Inventories = nullptr;
    const TArray<FGuid> BoundInventories = InventoryIds;
    for (const FGuid& InventoryId : BoundInventories)
    {
        if (Revision != ViewRevision || Network.Get() != SourceNetwork)
        {
            return;
        }
        const FInventorySnapshot* View = SourceNetwork->FindView(InventoryId);
        if (!View)
        {
            continue;
        }
        const TArray<FInventorySlotState> SnapshotSlots = View->Slots;
        UPanelWidget* InventoryHost = GetInventoryHost(InventoryId);
        if (!InventoryHost && GroupPanelNames.IsEmpty() && SlotHostPrefixes.IsEmpty() && !Inventories)
        {
            Inventories = bStackInventoriesVertically
                ? static_cast<UPanelWidget*>(WidgetTree->ConstructWidget<UVerticalBox>())
                : static_cast<UPanelWidget*>(WidgetTree->ConstructWidget<UHorizontalBox>());
            GeneratedViewRoots.Add(Inventories);
            InventoryContent->AddChildToVerticalBox(Inventories);
        }
        UVerticalBox* Column = !InventoryHost && Inventories ? WidgetTree->ConstructWidget<UVerticalBox>() : nullptr;
        if (Column)
        {
            Inventories->AddChild(Column);
        }
        UPanelWidget* InventoryDestination = InventoryHost ? InventoryHost : Column ? static_cast<UPanelWidget*>(Column) : InventoryContent.Get();
        if (bShowInventoryTitles && (InventoryHost || Column))
        {
            UCommonTextBlockN* InventoryTitle = WidgetTree->ConstructWidget<UCommonTextBlockN>();
            InventoryTitle->SetStyleTag(InventoryTitleStyleTag);
            InventoryTitle->SetText(InventoryId == SourceNetwork->GetOwnerInventoryId() ? NSLOCTEXT("WH.Inventory", "Owner", "背包") : NSLOCTEXT("WH.Inventory", "Container", "容器"));
            GeneratedViewRoots.Add(InventoryTitle);
            InventoryDestination->AddChild(InventoryTitle);
        }
        FGameplayTag LastGroup;
        UPanelWidget* Grid = nullptr;
        int32 Position = 0;
        for (const FInventorySlotState& SlotState : SnapshotSlots)
        {
            if (Revision != ViewRevision || Network.Get() != SourceNetwork)
            {
                return;
            }
            if (!VisibleGroups.IsEmpty() && !VisibleGroups.Contains(SlotState.Key.Group))
            {
                continue;
            }
            UPanelWidget* FixedHost = nullptr;
            if (const TMap<int32, TWeakObjectPtr<UPanelWidget>>* Hosts = GroupSlotHosts.Find(SlotState.Key.Group))
            {
                const TWeakObjectPtr<UPanelWidget>* Host = Hosts->Find(SlotState.Key.Index);
                if (!Host || !Host->IsValid())
                {
                    continue;
                }
                FixedHost = Host->Get();
            }
            else if (const FName* Prefix = SlotHostPrefixes.Find(SlotState.Key.Group))
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
                    Destination = InventoryDestination;
                }
                if (bShowGroupTitles)
                {
                    UCommonTextBlockN* Title = WidgetTree->ConstructWidget<UCommonTextBlockN>();
                    Title->SetStyleTag(GroupTitleStyleTag);
                    Title->SetText(GroupNames.Contains(LastGroup) ? GroupNames.FindChecked(LastGroup) : FText::FromString(LastGroup.ToString()));
                    GeneratedViewRoots.Add(Title);
                    Destination->AddChild(Title);
                }
                if (bFixedSize)
                {
                    Grid = WidgetTree->ConstructWidget<UCanvasPanel>();
                }
                else
                {
                    UUniformGridPanel* UniformGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
                    UniformGrid->SetSlotPadding(GridSlotPadding);
                    UniformGrid->SetMinDesiredSlotWidth(64.f);
                    UniformGrid->SetMinDesiredSlotHeight(64.f);
                    Grid = UniformGrid;
                }
                GeneratedViewRoots.Add(Grid);
                Destination->AddChild(Grid);
            }
            if (Revision != ViewRevision || Network.Get() != SourceNetwork)
            {
                return;
            }
            UWidgetAbilityInventorySlotBase* Widget = CreateWidget<UWidgetAbilityInventorySlotBase>(GetOwningPlayer(), SlotWidgetClass);
            if (Revision != ViewRevision || Network.Get() != SourceNetwork)
            {
                return;
            }
            if(!Widget) continue;
            Widget->BindSlot(this, InventoryId, SlotState.Key);
            ConfigureSlot(Widget, SlotState.Key);
            if (Revision != ViewRevision || Network.Get() != SourceNetwork)
            {
                return;
            }
            Slots.Add(SlotId(InventoryId, SlotState.Key), Widget);
            if (FixedHost)
            {
                if (UOverlaySlot* HostSlot = Cast<UOverlaySlot>(FixedHost->AddChild(Widget)))
                {
                    HostSlot->SetHorizontalAlignment(HAlign_Fill);
                    HostSlot->SetVerticalAlignment(VAlign_Fill);
                }
            }
            else
            {
                const int32 GroupColumnCount = FMath::Max(1, GetInventoryColumns(InventoryId, SlotState.Key.Group));
                if (UCanvasPanel* FixedGrid = Cast<UCanvasPanel>(Grid))
                {
                    UCanvasPanelSlot* CellSlot = FixedGrid->AddChildToCanvas(Widget);
                    CellSlot->SetPosition(FVector2D(
                        Position % GroupColumnCount * (GridSlotSize.X + GridSlotPadding.Left + GridSlotPadding.Right) + GridSlotPadding.Left,
                        Position / GroupColumnCount * (GridSlotSize.Y + GridSlotPadding.Top + GridSlotPadding.Bottom) + GridSlotPadding.Top));
                    CellSlot->SetSize(GridSlotSize);
                }
                else
                {
                    CastChecked<UUniformGridPanel>(Grid)->AddChildToUniformGrid(Widget, Position / GroupColumnCount, Position % GroupColumnCount);
                }
                ++Position;
            }
            if (Revision != ViewRevision || Network.Get() != SourceNetwork)
            {
                return;
            }
            RefreshSlot(InventoryId, SlotState);
        }
    }
    if (Revision == ViewRevision && Network.Get() == SourceNetwork)
    {
        RefreshPending();
    }
}

void UWidgetAbilityInventoryBase::ConfigureSlot(UWidgetAbilityInventorySlotBase* InWidget, const FInventorySlotKey& InSlot) const
{
}

int32 UWidgetAbilityInventoryBase::GetInventoryColumns(const FGuid& InInventoryId, const FGameplayTag& InGroup) const
{
    return FMath::Max(1, GroupColumns.Contains(InGroup) ? GroupColumns.FindChecked(InGroup) : Columns);
}

UPanelWidget* UWidgetAbilityInventoryBase::GetInventoryHost(const FGuid& InInventoryId) const
{
    return nullptr;
}

uint64 UWidgetAbilityInventoryBase::ClearViews()
{
    const uint64 Revision = ++ViewRevision;
    const TMap<FString, TWeakObjectPtr<UWidgetAbilityInventorySlotBase>> PreviousSlots = MoveTemp(Slots);
    const TArray<TWeakObjectPtr<UWidget>> PreviousRoots = MoveTemp(GeneratedViewRoots);
    for (const TPair<FString, TWeakObjectPtr<UWidgetAbilityInventorySlotBase>>& Entry : PreviousSlots)
    {
        if (Entry.Value.IsValid())
        {
            Entry.Value->RemoveFromParent();
        }
    }
    for (const TWeakObjectPtr<UWidget>& Root : PreviousRoots)
    {
        if (Root.IsValid())
        {
            Root->RemoveFromParent();
        }
    }
    RefreshControls();
    return Revision;
}

void UWidgetAbilityInventoryBase::RefreshSlot(const FGuid& InInventoryId, const FInventorySlotState& InSlot)
{
    const TWeakObjectPtr<UWidgetAbilityInventorySlotBase>* Found = Slots.Find(SlotId(InInventoryId, InSlot.Key));
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
    if (!PendingSlots.Contains(InResult.RequestId))
    {
        return;
    }
    if (InResult.Code != EInventoryResultCode::ConfirmationPending || !Network.IsValid() || !Network->IsRequestUnconfirmed(InResult.RequestId))
    {
        PendingSlots.Remove(InResult.RequestId);
    }
    RefreshPending();
    if (ResultText)
    {
        if(InResult.Code == EInventoryResultCode::ConfirmationPending)
        {
            SetResultText(NSLOCTEXT("WH.Inventory", "AwaitConfirmation", "结果待确认，请等待重新同步库存。"));
            return;
        }
        if (InResult.IsSuccess())
        {
            SetResultText(InResult.RemainingCount > 0 ? FText::Format(NSLOCTEXT("WH.Inventory", "Partial", "已处理 {0}，剩余 {1}"), FText::AsNumber(InResult.AcceptedCount), FText::AsNumber(InResult.RemainingCount)) : FText::GetEmpty());
        }
        else
        {
            const UEnum* Codes = StaticEnum<EInventoryResultCode>();
            SetResultText(FText::Format(NSLOCTEXT("WH.Inventory", "Rejected", "操作未完成：{0}"), Codes->GetDisplayNameTextByValue(static_cast<int64>(InResult.Code))));
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
            SetResultText(NSLOCTEXT("WH.Inventory", "Closed", "容器会话已关闭"));
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
    ApplySelection(InInventoryId, InSlot);
    OnSelection.Broadcast(InInventoryId, InSlot);
}

void UWidgetAbilityInventoryBase::ApplySelection(const FGuid& InInventoryId, const FInventorySlotKey& InSlot)
{
    const TWeakObjectPtr<UWidgetAbilityInventorySlotBase>* Previous = Slots.Find(SlotId(SelectedInventoryId, SelectedSlot));
    if (Previous && Previous->IsValid())
    {
        Previous->Get()->SetSelected(false);
    }
    SelectedInventoryId = InInventoryId;
    SelectedSlot = InSlot;
    const TWeakObjectPtr<UWidgetAbilityInventorySlotBase>* Current = Slots.Find(SlotId(InInventoryId, InSlot));
    if (Current && Current->IsValid())
    {
        Current->Get()->SetSelected(true);
    }
    RefreshControls();
}

TArray<FString> UWidgetAbilityInventoryBase::CollectRequestSlots(const FInventoryClientRequest& InRequest) const
{
    if (!Network.IsValid())
    {
        return {};
    }
    TArray<FString> Affected;
    if (InRequest.SourceSlot.IsValid() && InventoryIds.Contains(InRequest.SourceInventoryId))
    {
        Affected.Add(SlotId(InRequest.SourceInventoryId, InRequest.SourceSlot));
    }
    if (InRequest.TargetSlot.IsValid() && InventoryIds.Contains(InRequest.TargetInventoryId))
    {
        Affected.AddUnique(SlotId(InRequest.TargetInventoryId, InRequest.TargetSlot));
    }
    const auto AddInventorySlots = [&](const FGuid& InventoryId, const FGameplayTag& Group)
    {
        if (!InventoryIds.Contains(InventoryId)) return;
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
    return Affected;
}

void UWidgetAbilityInventoryBase::HandleRequestQueued(const FInventoryClientRequest& InRequest)
{
    if (!Network.IsValid() || InRequest.RequestId == 0 || InRequest.WorldEpoch != Network->GetWorldEpoch() || InRequest.StoreId != Network->GetStoreId()) return;
    for (const FInventoryClientParticipant& Participant : InRequest.Participants)
    {
        if (!InventoryIds.Contains(Participant.InventoryId)) continue;
        const FInventorySnapshot* View = Network->FindView(Participant.InventoryId);
        if (!View || View->Generation != Participant.Generation || Participant.SessionId != Network->GetSessionId(Participant.InventoryId)) return;
    }
    TArray<FString> Affected = CollectRequestSlots(InRequest);
    if (Affected.IsEmpty()) return;
    PendingSlots.Add(InRequest.RequestId, MoveTemp(Affected));
    RefreshPending();
}

bool UWidgetAbilityInventoryBase::Submit(FInventoryClientRequest InRequest)
{
    if (!Network.IsValid()) return false;
    const TArray<FString> Affected = CollectRequestSlots(InRequest);
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
    const TWeakObjectPtr<UInventoryNetworkComponent> SubmittedNetwork = Network;
    const uint64 RequestId = SubmittedNetwork->SubmitRequest(MoveTemp(InRequest));
    return RequestId != 0 && Network == SubmittedNetwork;
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
    for (const TPair<FString, TWeakObjectPtr<UWidgetAbilityInventorySlotBase>>& Pair : Slots)
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
    const FGuid* Target = TransferInventoryId.IsValid() && TransferInventoryId != InInventoryId
        ? &TransferInventoryId
        : InventoryIds.FindByPredicate([&](const FGuid& Candidate) { return Candidate != InInventoryId; });
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
        return OpenSplit(Request);
    }
    return Submit(Request);
}

bool UWidgetAbilityInventoryBase::CanSplit(FGuid InInventoryId, FInventorySlotKey InSlot) const
{
    const FInventorySnapshot* View = Network.IsValid() ? Network->FindView(InInventoryId) : nullptr;
    const FInventorySlotState* Source = View ? View->FindSlot(InSlot) : nullptr;
    if (!Source || Source->bLocked || Source->Item.Count <= 1 || Source->Item.InstanceId.IsValid())
    {
        return false;
    }
    return View->Slots.ContainsByPredicate([&InSlot](const FInventorySlotState& Candidate)
    {
        return Candidate.Key.Group == InSlot.Group && Candidate.Key != InSlot && !Candidate.bLocked && Candidate.Item.IsEmpty();
    });
}

bool UWidgetAbilityInventoryBase::RequestSplit(FGuid InInventoryId, FInventorySlotKey InSlot)
{
    if (!CanSplit(InInventoryId, InSlot))
    {
        return false;
    }
    const FInventorySnapshot* View = Network->FindView(InInventoryId);
    const FInventorySlotState* Target = View->Slots.FindByPredicate([&InSlot](const FInventorySlotState& Candidate)
    {
        return Candidate.Key.Group == InSlot.Group && Candidate.Key != InSlot && !Candidate.bLocked && Candidate.Item.IsEmpty();
    });
    FInventoryClientRequest Request;
    Request.SourceInventoryId = InInventoryId;
    Request.TargetInventoryId = InInventoryId;
    Request.SourceSlot = InSlot;
    Request.TargetSlot = Target->Key;
    return OpenSplit(Request);
}

bool UWidgetAbilityInventoryBase::OpenSplit(const FInventoryClientRequest& InRequest)
{
    const FInventorySlotState* Source = FindSlot(InRequest.SourceInventoryId, InRequest.SourceSlot);
    const FInventorySlotState* Target = FindSlot(InRequest.TargetInventoryId, InRequest.TargetSlot);
    if (!Network.IsValid() || !Source || !Target || Source->bLocked || Target->bLocked || !Target->Item.IsEmpty() ||
        Source->Item.Count <= 1 || Source->Item.InstanceId.IsValid())
    {
        return false;
    }
    const int32 StackCount = Source->Item.Count;
    const FInventoryStackSignature Signature = Source->Item.Signature();
    CancelSplit();
    SplitRequest = InRequest;
    SplitRequest.Operation = EInventoryOperationType::Split;
    SplitRequest.bAllowPartial = false;
    SplitSignature = Signature;
    SplitWorldEpoch = Network->GetWorldEpoch();
    SplitSourceSessionId = Network->GetSessionId(InRequest.SourceInventoryId);
    SplitTargetSessionId = Network->GetSessionId(InRequest.TargetInventoryId);
    if (!ShowSplitQuantity(StackCount))
    {
        CancelSplit();
        return false;
    }
    return true;
}

void UWidgetAbilityInventoryBase::ConfirmSplitQuantity(int32 InQuantity)
{
    if (SplitRequest.Operation != EInventoryOperationType::Split)
    {
        return;
    }
    FInventoryClientRequest Request = SplitRequest;
    Request.Quantity = InQuantity;
    const FInventorySlotState* Source = FindSlot(Request.SourceInventoryId, Request.SourceSlot);
    const FInventorySlotState* Target = FindSlot(Request.TargetInventoryId, Request.TargetSlot);
    const bool bMatches = Network.IsValid() && SplitWorldEpoch == Network->GetWorldEpoch() &&
        SplitSourceSessionId.IsValid() && SplitSourceSessionId == Network->GetSessionId(Request.SourceInventoryId) &&
        SplitTargetSessionId.IsValid() && SplitTargetSessionId == Network->GetSessionId(Request.TargetInventoryId) &&
        Source && Target && !Source->bLocked && !Target->bLocked && Source->Item.Signature() == SplitSignature &&
        Request.Quantity > 0 && Request.Quantity < Source->Item.Count && Target->Item.IsEmpty();
    CancelSplit();
    if (bMatches)
    {
        Submit(Request);
    }
    else if (ResultText)
    {
        SetResultText(NSLOCTEXT("WH.Inventory", "SplitChanged", "拆分期间物品已变化，请重新选择。"));
    }
}

bool UWidgetAbilityInventoryBase::ShowSplitQuantity(int32 InStackCount)
{
    return false;
}

void UWidgetAbilityInventoryBase::CloseSplitQuantity()
{
}

void UWidgetAbilityInventoryBase::CancelSplit()
{
    SplitRequest = FInventoryClientRequest();
    SplitSignature = FInventoryStackSignature();
    SplitWorldEpoch.Invalidate();
    SplitSourceSessionId.Invalidate();
    SplitTargetSessionId.Invalidate();
    CloseSplitQuantity();
}

void UWidgetAbilityInventoryBase::Navigate(const FGuid& InInventoryId, const FInventorySlotKey& InSlot, int32 InOffset)
{
    FInventorySlotKey Target = InSlot;
    const TWeakObjectPtr<UWidgetAbilityInventorySlotBase>* Source = Slots.Find(SlotId(InInventoryId, InSlot));
    const bool bDirectional = InOffset == -1 || InOffset == 1 || InOffset == -10 || InOffset == 10;
    if (bDirectional && (SlotHostPrefixes.Contains(InSlot.Group) || GroupSlotHosts.Contains(InSlot.Group)) && Source && Source->IsValid() &&
        Source->Get()->GetCachedGeometry().GetLocalSize().X > 0.f && Source->Get()->GetCachedGeometry().GetLocalSize().Y > 0.f)
    {
        const FGeometry& Geometry = Source->Get()->GetCachedGeometry();
        const FVector2D Origin = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
        const FVector2D Direction = FMath::Abs(InOffset) == 10 ? FVector2D(0.f, FMath::Sign(InOffset)) : FVector2D(FMath::Sign(InOffset), 0.f);
        double BestScore = TNumericLimits<double>::Max();
        bool bFound = false;
        for (const TPair<FString, TWeakObjectPtr<UWidgetAbilityInventorySlotBase>>& Entry : Slots)
        {
            const UWidgetAbilityInventorySlotBase* Candidate = Entry.Value.Get();
            if (!Candidate || !Candidate->IsVisible() || !Candidate->GetIsEnabled() || Candidate->GetInventoryId() != InInventoryId)
            {
                continue;
            }
            const FInventorySlotKey CandidateSlot = Candidate->GetSlotKey();
            const FGeometry& CandidateGeometry = Candidate->GetCachedGeometry();
            if (CandidateSlot.Group != InSlot.Group || CandidateSlot == InSlot ||
                CandidateGeometry.GetLocalSize().X <= 0.f || CandidateGeometry.GetLocalSize().Y <= 0.f)
            {
                continue;
            }
            const FVector2D Difference = CandidateGeometry.LocalToAbsolute(CandidateGeometry.GetLocalSize() * 0.5) - Origin;
            const double Forward = FVector2D::DotProduct(Difference, Direction);
            const double Lateral = FMath::Abs(Difference.X * Direction.Y - Difference.Y * Direction.X);
            const double Score = Forward + Lateral * 3.0;
            if (Forward > UE_SMALL_NUMBER && FMath::IsFinite(Score) &&
                (Score < BestScore || (Score == BestScore && CandidateSlot.Index < Target.Index)))
            {
                BestScore = Score;
                Target = CandidateSlot;
                bFound = true;
            }
        }
        if (!bFound)
        {
            return;
        }
    }
    else
    {
        const int32 GroupColumnCount = FMath::Max(1, GetInventoryColumns(InInventoryId, InSlot.Group));
        Target.Index += InOffset == -10 ? -GroupColumnCount : (InOffset == 10 ? GroupColumnCount : InOffset);
    }
    const TWeakObjectPtr<UWidgetAbilityInventorySlotBase>* Widget = Slots.Find(SlotId(InInventoryId, Target));
    if (Widget && Widget->IsValid())
    {
        const TWeakObjectPtr<UWidgetAbilityInventorySlotBase> FocusTarget = *Widget;
        SelectSlot(InInventoryId, Target);
        const TWeakObjectPtr<UWidgetAbilityInventorySlotBase>* Current = Slots.Find(SlotId(InInventoryId, Target));
        if (Current && *Current == FocusTarget && FocusTarget.IsValid())
        {
            FocusTarget->SetUserFocus(GetOwningPlayer());
        }
    }
}

void UWidgetAbilityInventoryBase::UpdateCooldown(const FGuid& InInventoryId, const FInventorySlotKey& InSlot, float InRemaining, float InDuration)
{
    const TWeakObjectPtr<UWidgetAbilityInventorySlotBase>* Widget = Slots.Find(SlotId(InInventoryId, InSlot));
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

void UWidgetAbilityInventoryBase::SetTransferInventoryId(FGuid InInventoryId)
{
    TransferInventoryId = InInventoryId;
}

void UWidgetAbilityInventoryBase::SyncSelection(FGuid InInventoryId, FInventorySlotKey InSlot)
{
    ApplySelection(InInventoryId, InSlot);
}

bool UWidgetAbilityInventoryBase::SubmitRequest(FInventoryClientRequest InRequest)
{
    return Submit(MoveTemp(InRequest));
}

void UWidgetAbilityInventoryBase::SetResultText(const FText& InText)
{
    if (ResultText)
    {
        ResultText->SetText(InText);
        ResultText->SetRenderOpacity(1.f);
        ResultText->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        ResultFadeAt = !InText.IsEmpty() && ResultDisplayTime > 0.f ? FPlatformTime::Seconds() + ResultDisplayTime : 0.0;
        ResumeResultFade();
    }
}

void UWidgetAbilityInventoryBase::ResumeResultFade()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ResultFadeTimer);
        if (ResultFadeAt > 0.0)
        {
            UpdateResultFade();
            if (ResultFadeAt > 0.0)
            {
                World->GetTimerManager().SetTimer(ResultFadeTimer, this, &ThisClass::UpdateResultFade, .05f, true);
            }
        }
    }
}

void UWidgetAbilityInventoryBase::UpdateResultFade()
{
    const float Opacity = FMath::Clamp(static_cast<float>((ResultFadeAt + FMath::Max(.01f, ResultFadeTime) - FPlatformTime::Seconds()) / FMath::Max(.01f, ResultFadeTime)), 0.f, 1.f);
    ResultText->SetRenderOpacity(Opacity);
    if (Opacity <= 0.f)
    {
        ResultText->SetVisibility(ESlateVisibility::Collapsed);
        ResultFadeAt = 0.0;
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ResultFadeTimer);
    }
}
