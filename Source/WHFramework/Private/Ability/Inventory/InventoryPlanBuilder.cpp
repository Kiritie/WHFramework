#include "InventoryPlanBuilder.h"

FInventoryPlanBuilder::FInventoryPlanBuilder(const FInventoryPlannerContext& InContext)
    : Context(InContext)
{
}

EInventoryResultCode FInventoryPlanBuilder::Build(TConstArrayView<FInventorySnapshot> InStates, TConstArrayView<FInventoryOperation> InOperations, FInventoryPlan& OutPlan, FString& OutError)
{
    OutPlan = FInventoryPlan();
    OutError.Reset();
    if (InOperations.Num() > 4096)
    {
        OutPlan.Code = EInventoryResultCode::InvalidRequest;
        OutError = TEXT("单笔事务超过操作数上限。");
        return OutPlan.Code;
    }
    EInventoryResultCode Code = Initialize(InStates, OutError);
    if (Code != EInventoryResultCode::Success)
    {
        OutPlan.Code = Code;
        return Code;
    }
    for (const FInventoryOperation& Operation : InOperations)
    {
        FInventoryOperationResult Result;
        Code = Apply(Operation, Result);
        if (Code != EInventoryResultCode::Success && Code != EInventoryResultCode::Partial && Code != EInventoryResultCode::NoChange)
        {
            OutPlan = FInventoryPlan();
            OutPlan.Code = Code;
            OutError = FString::Printf(TEXT("库存纯规划被拒绝，结果代码为 %d。"), static_cast<int32>(Code));
            return Code;
        }
        Result.Code = Code;
        Plan.AcceptedCount += Result.AcceptedCount;
        Plan.RemainingCount += Result.RemainingCount;
        Plan.Operations.Add(Result);
    }
    Code = Finalize(OutPlan, OutError);
    if (Code != EInventoryResultCode::Success && Code != EInventoryResultCode::Partial && Code != EInventoryResultCode::NoChange)
    {
        OutPlan = FInventoryPlan();
        OutPlan.Code = Code;
    }
    return Code;
}

EInventoryResultCode FInventoryPlanBuilder::Initialize(TConstArrayView<FInventorySnapshot> InStates, FString& OutError)
{
    if (!Context.Registry.IsInitialized() || InStates.Num() > 128)
    {
        OutError = TEXT("库存规划上下文尚未就绪或参与库存过多。");
        return EInventoryResultCode::InvalidState;
    }
    Original.Reset(InStates.Num());
    for (const FInventorySnapshot& State : InStates)
    {
        Original.Add(State);
    }
    Original.Sort([](const FInventorySnapshot& InLeft, const FInventorySnapshot& InRight)
    {
        return InventoryOrder::GuidLess(InLeft.InventoryId, InRight.InventoryId);
    });
    FGuid WorldId;
    TSet<FPrimaryAssetId> ValidatedProfiles;
    for (int32 StateIndex = 0; StateIndex < Original.Num(); ++StateIndex)
    {
        const FInventorySnapshot& State = Original[StateIndex];
        const FInventoryProfileDefinition* Profile = Context.Profiles.Find(State.ProfileId);
        if (!Profile || StateIndices.Contains(State.InventoryId))
        {
            OutError = TEXT("库存布局缺失或参与库存身份重复。");
            return EInventoryResultCode::InvalidState;
        }
        if (!ValidatedProfiles.Contains(State.ProfileId))
        {
            if (!Profile->ValidateDefinition(OutError))
            {
                return EInventoryResultCode::InvalidState;
            }
            ValidatedProfiles.Add(State.ProfileId);
        }
        if (!Profile->ValidateState(State, Context.Registry, OutError))
        {
            return EInventoryResultCode::InvalidState;
        }
        if (WorldId.IsValid() && WorldId != State.WorldId)
        {
            OutError = TEXT("一笔库存事务不能跨世界保存域。");
            return EInventoryResultCode::InvalidState;
        }
        WorldId = State.WorldId;
        StateIndices.Add(State.InventoryId, StateIndex);
    }
    if (!FInventoryPlanner::ValidateUniqueInstances(Original, OutError))
    {
        return EInventoryResultCode::DuplicateInstance;
    }
    Working = Original;
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::Apply(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    if (InOperation.Type == EInventoryOperationType::None
        || static_cast<uint8>(InOperation.Type) > static_cast<uint8>(EInventoryOperationType::Transform)
        || (InOperation.AmountMode != EInventoryAmountMode::Exact && InOperation.AmountMode != EInventoryAmountMode::UpTo)
        || InOperation.SourceGroups.Num() > 64 || InOperation.TargetGroups.Num() > 64
        || InOperation.TargetInventories.Num() > 128
        || (InOperation.Filter.bMatchLevel && InOperation.Filter.Level < 0))
    {
        return EInventoryResultCode::InvalidRequest;
    }
    if (InOperation.Type != EInventoryOperationType::Sort && InOperation.Type != EInventoryOperationType::SetFavorite
        && InOperation.Type != EInventoryOperationType::Swap && InOperation.Type != EInventoryOperationType::QuickStack
        && InOperation.Requested <= 0)
    {
        return EInventoryResultCode::InvalidCount;
    }
    switch (InOperation.Type)
    {
        case EInventoryOperationType::Add:
        {
            const int32 TargetIndex = FindState(InOperation.TargetInventoryId);
            if (TargetIndex == INDEX_NONE)
            {
                return EInventoryResultCode::InventoryGone;
            }
            int32 Accepted = 0;
            const EInventoryResultCode Code = Add(TargetIndex, InOperation.Item, InOperation.Requested,
                InOperation.AmountMode, InOperation, false, Accepted);
            if (Code == EInventoryResultCode::Success || Code == EInventoryResultCode::Partial)
            {
                TrackQuantity(InOperation.Item, Accepted, 0);
                OutResult.AcceptedCount = Accepted;
                OutResult.RemainingCount = static_cast<int64>(InOperation.Requested) - Accepted;
            }
            return Code;
        }
        case EInventoryOperationType::Remove:
        {
            const int32 SourceIndex = FindState(InOperation.SourceInventoryId);
            return SourceIndex == INDEX_NONE ? EInventoryResultCode::InventoryGone : Remove(SourceIndex, InOperation, OutResult);
        }
        case EInventoryOperationType::Move:
        case EInventoryOperationType::Split:
        case EInventoryOperationType::Merge:
            return Move(InOperation, OutResult);
        case EInventoryOperationType::Transfer:
            return Transfer(InOperation, OutResult);
        case EInventoryOperationType::Exchange:
            return Exchange(InOperation, OutResult);
        case EInventoryOperationType::Swap:
            return Swap(InOperation, OutResult);
        case EInventoryOperationType::Sort:
            return Sort(InOperation, OutResult);
        case EInventoryOperationType::QuickStack:
            return QuickStack(InOperation, OutResult);
        case EInventoryOperationType::SetFavorite:
            return SetFavorite(InOperation, OutResult);
        case EInventoryOperationType::Transform:
            return Transform(InOperation, OutResult);
        default:
            return EInventoryResultCode::InvalidRequest;
    }
}

int32 FInventoryPlanBuilder::FindState(const FGuid& InInventoryId) const
{
    const int32* StateIndex = StateIndices.Find(InInventoryId);
    return StateIndex ? *StateIndex : INDEX_NONE;
}

int32 FInventoryPlanBuilder::FindSlot(const int32 InStateIndex, const FInventorySlotKey& InKey) const
{
    return Working[InStateIndex].Slots.IndexOfByPredicate([&InKey](const FInventorySlotState& InSlot)
    {
        return InSlot.Key == InKey;
    });
}

const FInventoryProfileDefinition& FInventoryPlanBuilder::GetProfile(const int32 InStateIndex) const
{
    return Context.Profiles.FindChecked(Working[InStateIndex].ProfileId);
}

const FInventoryRuleProvider& FInventoryPlanBuilder::GetRules() const
{
    return Context.Rules ? *Context.Rules : DefaultRules;
}

EInventoryResultCode FInventoryPlanBuilder::GetCandidates(const int32 InStateIndex, const FInventoryOperation& InOperation, const bool bInTarget, TArray<int32>& OutIndices) const
{
    OutIndices.Reset();
    const FInventorySlotKey& SpecificSlot = bInTarget ? InOperation.TargetSlot : InOperation.SourceSlot;
    const TArray<FGameplayTag>& Groups = bInTarget ? InOperation.TargetGroups : InOperation.SourceGroups;
    const bool bSpecified = SpecificSlot.Group.IsValid() || SpecificSlot.Index != INDEX_NONE;
    if (bSpecified && !SpecificSlot.IsValid())
    {
        return EInventoryResultCode::InvalidSlot;
    }
    TSet<FGameplayTag> SeenGroups;
    for (const FGameplayTag Group : Groups)
    {
        if (!Group.IsValid() || SeenGroups.Contains(Group))
        {
            return EInventoryResultCode::InvalidRequest;
        }
        SeenGroups.Add(Group);
        const bool bGroupExists = GetProfile(InStateIndex).Slots.ContainsByPredicate([Group](const FInventorySlotDefinition& InSlot)
        {
            return InSlot.Key.Group == Group;
        });
        if (!bGroupExists)
        {
            return EInventoryResultCode::InvalidSlot;
        }
    }
    if (bSpecified)
    {
        const int32 SlotIndex = FindSlot(InStateIndex, SpecificSlot);
        if (SlotIndex == INDEX_NONE || (!Groups.IsEmpty() && !Groups.Contains(SpecificSlot.Group)))
        {
            return EInventoryResultCode::InvalidSlot;
        }
        OutIndices.Add(SlotIndex);
        return EInventoryResultCode::Success;
    }
    const FInventoryProfileDefinition& Profile = GetProfile(InStateIndex);
    for (int32 SlotIndex = 0; SlotIndex < Profile.Slots.Num(); ++SlotIndex)
    {
        const FInventorySlotDefinition& Definition = Profile.Slots[SlotIndex];
        if (!Groups.IsEmpty() && !Groups.Contains(Definition.Key.Group))
        {
            continue;
        }
        if (bInTarget && Working[InStateIndex].InventoryId == InOperation.SourceInventoryId
            && InOperation.SourceSlot.IsValid() && Definition.Key == InOperation.SourceSlot)
        {
            continue;
        }
        OutIndices.Add(SlotIndex);
    }
    OutIndices.Sort([&Profile, bInTarget](const int32 InLeft, const int32 InRight)
    {
        const int32 LeftPriority = bInTarget ? Profile.Slots[InLeft].AddPriority : Profile.Slots[InLeft].RemovePriority;
        const int32 RightPriority = bInTarget ? Profile.Slots[InRight].AddPriority : Profile.Slots[InRight].RemovePriority;
        return LeftPriority != RightPriority ? LeftPriority < RightPriority : InLeft < InRight;
    });
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::CanTake(const int32 InStateIndex, const int32 InSlotIndex, const FInventoryOperation& InOperation) const
{
    const FInventorySnapshot& State = Working[InStateIndex];
    const FInventorySlotState& Slot = State.Slots[InSlotIndex];
    const FInventorySlotRule& Rule = GetProfile(InStateIndex).Slots[InSlotIndex].Rule;
    if (Slot.bLocked)
    {
        return EInventoryResultCode::ItemLocked;
    }
    if (!Rule.bAllowsRemove || (InOperation.bAutomatic && (!Rule.bAllowsAutomatic || Slot.bFavorite)))
    {
        return EInventoryResultCode::SlotRuleRejected;
    }
    return GetRules().CanTake(State, Slot, InOperation, Context.Registry);
}

EInventoryResultCode FInventoryPlanBuilder::CanPut(const int32 InStateIndex, const int32 InSlotIndex, const FAbilityItem& InItem, const FInventoryOperation& InOperation) const
{
    const FInventorySnapshot& State = Working[InStateIndex];
    const FInventorySlotState& Slot = State.Slots[InSlotIndex];
    const FInventorySlotRule& Rule = GetProfile(InStateIndex).Slots[InSlotIndex].Rule;
    if (Slot.bLocked)
    {
        return EInventoryResultCode::ItemLocked;
    }
    const FInventoryItemDefinition* Definition = Context.Registry.Find(InItem.ItemId);
    if (!Definition || !Rule.bAllowsInsert || !Rule.Accepts(*Definition)
        || (InOperation.bAutomatic && (!Rule.bAllowsAutomatic || Slot.bFavorite)))
    {
        return EInventoryResultCode::SlotRuleRejected;
    }
    return GetRules().CanPut(State, Slot, InItem, InOperation, Context.Registry);
}

void FInventoryPlanBuilder::ReduceSlot(const int32 InStateIndex, const int32 InSlotIndex, const int32 InCount)
{
    FInventorySlotState& Slot = Working[InStateIndex].Slots[InSlotIndex];
    Slot.Item.Count -= InCount;
    if (Slot.Item.Count == 0)
    {
        Slot.Item = FAbilityItem();
        Slot.bFavorite = false;
    }
}

void FInventoryPlanBuilder::TrackQuantity(const FAbilityItem& InItem, const int64 InMinted, const int64 InConsumed)
{
    const FInventoryStackSignature Signature = InItem.Signature();
    FInventoryQuantityDelta* Existing = Plan.QuantityDeltas.FindByPredicate([&Signature](const FInventoryQuantityDelta& InDelta)
    {
        return InDelta.Signature == Signature;
    });
    if (Existing)
    {
        Existing->Minted += InMinted;
        Existing->Consumed += InConsumed;
        return;
    }
    FInventoryQuantityDelta Delta;
    Delta.Signature = Signature;
    Delta.Minted = InMinted;
    Delta.Consumed = InConsumed;
    Plan.QuantityDeltas.Add(MoveTemp(Delta));
}

EInventoryResultCode FInventoryPlanBuilder::Add(const int32 InTargetIndex, const FAbilityItem& InItem, const int32 InRequested, const EInventoryAmountMode InMode, const FInventoryOperation& InOperation, const bool bInFavorite, int32& OutAccepted)
{
    OutAccepted = 0;
    FString Error;
    if (InItem.Count != 1 || InItem.IsEmpty() || !Context.Registry.ValidateItem(InItem, Error))
    {
        return EInventoryResultCode::InvalidItem;
    }
    if (InRequested < 1 || (InItem.InstanceId.IsValid() && InRequested != 1))
    {
        return EInventoryResultCode::InvalidCount;
    }
    TArray<int32> Candidates;
    EInventoryResultCode Code = GetCandidates(InTargetIndex, InOperation, true, Candidates);
    if (Code != EInventoryResultCode::Success)
    {
        return Code;
    }
    if (InOperation.TargetSlot.IsValid())
    {
        Code = CanPut(InTargetIndex, Candidates[0], InItem, InOperation);
        if (Code != EInventoryResultCode::Success)
        {
            return Code;
        }
        const FAbilityItem& Existing = Working[InTargetIndex].Slots[Candidates[0]].Item;
        if (!Existing.IsEmpty() && !Existing.CanStackWith(InItem))
        {
            return EInventoryResultCode::StackMismatch;
        }
    }
    const FInventoryItemDefinition& Definition = *Context.Registry.Find(InItem.ItemId);
    int32 Remaining = InRequested;
    for (int32 PassIndex = 0; PassIndex < 2 && Remaining > 0; ++PassIndex)
    {
        for (const int32 SlotIndex : Candidates)
        {
            FInventorySlotState& Slot = Working[InTargetIndex].Slots[SlotIndex];
            const bool bEmpty = Slot.Item.IsEmpty();
            if ((PassIndex == 0 && (bEmpty || !Slot.Item.CanStackWith(InItem)))
                || (PassIndex == 1 && !bEmpty)
                || CanPut(InTargetIndex, SlotIndex, InItem, InOperation) != EInventoryResultCode::Success)
            {
                continue;
            }
            const int32 Capacity = GetProfile(InTargetIndex).Slots[SlotIndex].Rule.GetCapacity(Definition);
            const int32 ExistingCount = bEmpty ? 0 : Slot.Item.Count;
            const int32 Quantity = FMath::Min(Remaining, Capacity - ExistingCount);
            if (Quantity <= 0)
            {
                continue;
            }
            if (bEmpty)
            {
                Slot.Item = InItem;
                Slot.Item.Count = Quantity;
                Slot.bFavorite = bInFavorite;
            }
            else
            {
                Slot.Item.Count += Quantity;
                Slot.bFavorite = Slot.bFavorite || bInFavorite;
            }
            Remaining -= Quantity;
            if (Remaining == 0)
            {
                break;
            }
        }
    }
    if ((InMode == EInventoryAmountMode::Exact && Remaining > 0) || Remaining == InRequested)
    {
        return EInventoryResultCode::NoCapacity;
    }
    OutAccepted = InRequested - Remaining;
    return Remaining > 0 ? EInventoryResultCode::Partial : EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::Remove(const int32 InSourceIndex, const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    TArray<int32> Candidates;
    EInventoryResultCode Code = GetCandidates(InSourceIndex, InOperation, false, Candidates);
    if (Code != EInventoryResultCode::Success)
    {
        return Code;
    }
    int64 Available = 0;
    TArray<int32> Eligible;
    for (const int32 SlotIndex : Candidates)
    {
        const FInventorySlotState& Slot = Working[InSourceIndex].Slots[SlotIndex];
        if (!InOperation.Filter.Matches(Slot.Item))
        {
            continue;
        }
        Code = CanTake(InSourceIndex, SlotIndex, InOperation);
        if (Code != EInventoryResultCode::Success)
        {
            if (InOperation.SourceSlot.IsValid())
            {
                return Code;
            }
            continue;
        }
        Available += Slot.Item.Count;
        Eligible.Add(SlotIndex);
    }
    if (Available == 0 || (InOperation.AmountMode == EInventoryAmountMode::Exact && Available < InOperation.Requested))
    {
        return EInventoryResultCode::InsufficientItems;
    }
    const int32 Accepted = static_cast<int32>(FMath::Min<int64>(Available, InOperation.Requested));
    int32 Remaining = Accepted;
    for (const int32 SlotIndex : Eligible)
    {
        const FAbilityItem Item = Working[InSourceIndex].Slots[SlotIndex].Item;
        const int32 Quantity = FMath::Min(Item.Count, Remaining);
        TrackQuantity(Item, 0, Quantity);
        ReduceSlot(InSourceIndex, SlotIndex, Quantity);
        Remaining -= Quantity;
        if (Remaining == 0)
        {
            break;
        }
    }
    OutResult.AcceptedCount = Accepted;
    OutResult.RemainingCount = static_cast<int64>(InOperation.Requested) - Accepted;
    return OutResult.RemainingCount > 0 ? EInventoryResultCode::Partial : EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::Move(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    const int32 SourceIndex = FindState(InOperation.SourceInventoryId);
    const int32 TargetIndex = FindState(InOperation.TargetInventoryId);
    if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InventoryGone;
    }
    const int32 SourceSlotIndex = FindSlot(SourceIndex, InOperation.SourceSlot);
    const int32 TargetSlotIndex = FindSlot(TargetIndex, InOperation.TargetSlot);
    if (!InOperation.SourceSlot.IsValid() || !InOperation.TargetSlot.IsValid()
        || SourceSlotIndex == INDEX_NONE || TargetSlotIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InvalidSlot;
    }
    if (SourceIndex == TargetIndex && SourceSlotIndex == TargetSlotIndex)
    {
        return EInventoryResultCode::NoChange;
    }
    const FAbilityItem& SourceItem = Working[SourceIndex].Slots[SourceSlotIndex].Item;
    const FAbilityItem& TargetItem = Working[TargetIndex].Slots[TargetSlotIndex].Item;
    if (SourceItem.IsEmpty() || !InOperation.Filter.Matches(SourceItem))
    {
        return EInventoryResultCode::InsufficientItems;
    }
    if (InOperation.Requested > SourceItem.Count)
    {
        return EInventoryResultCode::InvalidCount;
    }
    if (InOperation.Type == EInventoryOperationType::Split
        && (!TargetItem.IsEmpty() || SourceItem.InstanceId.IsValid()
            || InOperation.Requested >= SourceItem.Count || InOperation.AmountMode != EInventoryAmountMode::Exact))
    {
        return EInventoryResultCode::InvalidRequest;
    }
    if (InOperation.Type == EInventoryOperationType::Merge
        && (TargetItem.IsEmpty() || !SourceItem.CanStackWith(TargetItem)))
    {
        return EInventoryResultCode::StackMismatch;
    }
    return Transfer(InOperation, OutResult);
}

EInventoryResultCode FInventoryPlanBuilder::Transfer(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    const int32 SourceIndex = FindState(InOperation.SourceInventoryId);
    const int32 TargetIndex = FindState(InOperation.TargetInventoryId);
    if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InventoryGone;
    }
    if (SourceIndex == TargetIndex)
    {
        if (!InOperation.SourceSlot.IsValid())
        {
            return EInventoryResultCode::InvalidRequest;
        }
        if (InOperation.TargetSlot.IsValid() && InOperation.SourceSlot == InOperation.TargetSlot)
        {
            return EInventoryResultCode::NoChange;
        }
    }
    TArray<int32> SourceCandidates;
    EInventoryResultCode Code = GetCandidates(SourceIndex, InOperation, false, SourceCandidates);
    if (Code != EInventoryResultCode::Success)
    {
        return Code;
    }
    TArray<int32> TargetCandidates;
    Code = GetCandidates(TargetIndex, InOperation, true, TargetCandidates);
    if (Code != EInventoryResultCode::Success)
    {
        return Code;
    }
    TArray<int32> Eligible;
    int64 Available = 0;
    for (const int32 SlotIndex : SourceCandidates)
    {
        const FInventorySlotState& Slot = Working[SourceIndex].Slots[SlotIndex];
        if (!InOperation.Filter.Matches(Slot.Item))
        {
            continue;
        }
        Code = CanTake(SourceIndex, SlotIndex, InOperation);
        if (Code != EInventoryResultCode::Success)
        {
            if (InOperation.SourceSlot.IsValid())
            {
                return Code;
            }
            continue;
        }
        Available += Slot.Item.Count;
        Eligible.Add(SlotIndex);
    }
    if (Available == 0 || (InOperation.AmountMode == EInventoryAmountMode::Exact && Available < InOperation.Requested))
    {
        return EInventoryResultCode::InsufficientItems;
    }
    int32 Remaining = InOperation.Requested;
    for (const int32 SlotIndex : Eligible)
    {
        const FInventorySlotState SourceSlot = Working[SourceIndex].Slots[SlotIndex];
        if (SourceSlot.Item.IsEmpty())
        {
            continue;
        }
        const FGuid BoundOwner = SourceSlot.Item.GetBoundOwner();
        if (BoundOwner.IsValid() && Working[TargetIndex].OwnerId != BoundOwner)
        {
            if (InOperation.SourceSlot.IsValid())
            {
                return EInventoryResultCode::ItemBound;
            }
            continue;
        }
        FInventoryOperation InsertOperation = InOperation;
        InsertOperation.SourceSlot = SourceSlot.Key;
        FAbilityItem ItemTemplate = SourceSlot.Item;
        ItemTemplate.Count = 1;
        const int32 Requested = FMath::Min(Remaining, SourceSlot.Item.Count);
        int32 Accepted = 0;
        Code = Add(TargetIndex, ItemTemplate, Requested, EInventoryAmountMode::UpTo,
            InsertOperation, SourceSlot.bFavorite, Accepted);
        if (Code == EInventoryResultCode::Success || Code == EInventoryResultCode::Partial)
        {
            ReduceSlot(SourceIndex, SlotIndex, Accepted);
            Remaining -= Accepted;
            if (Remaining == 0)
            {
                break;
            }
        }
        else if (InOperation.SourceSlot.IsValid() || InOperation.TargetSlot.IsValid())
        {
            return Code;
        }
        else if (Code != EInventoryResultCode::NoCapacity && Code != EInventoryResultCode::StackMismatch
            && Code != EInventoryResultCode::SlotRuleRejected && Code != EInventoryResultCode::ItemBound
            && Code != EInventoryResultCode::ItemLocked)
        {
            return Code;
        }
    }
    if ((InOperation.AmountMode == EInventoryAmountMode::Exact && Remaining > 0) || Remaining == InOperation.Requested)
    {
        return EInventoryResultCode::NoCapacity;
    }
    OutResult.AcceptedCount = static_cast<int64>(InOperation.Requested) - Remaining;
    OutResult.RemainingCount = Remaining;
    return Remaining > 0 ? EInventoryResultCode::Partial : EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::Exchange(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    if (InOperation.AmountMode != EInventoryAmountMode::Exact || InOperation.ExchangeLines.IsEmpty()
        || InOperation.ExchangeLines.Num() > 128)
    {
        return EInventoryResultCode::InvalidRequest;
    }
    struct FReservedTransfer
    {
        int32 TargetIndex = INDEX_NONE;
        FInventoryOperation Operation;
        FAbilityItem Item;
        bool bFavorite = false;
    };
    TArray<FReservedTransfer> Transfers;
    for (const FInventoryExchangeLine& Line : InOperation.ExchangeLines)
    {
        if (Line.Requested <= 0 || Line.SourceGroups.Num() > 64 || Line.TargetGroups.Num() > 64)
        {
            return EInventoryResultCode::InvalidCount;
        }
        const int32 SourceIndex = FindState(Line.SourceInventoryId);
        const int32 TargetIndex = FindState(Line.TargetInventoryId);
        if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE)
        {
            return EInventoryResultCode::InventoryGone;
        }
        FInventoryOperation Operation;
        Operation.Type = EInventoryOperationType::Exchange;
        Operation.SourceInventoryId = Line.SourceInventoryId;
        Operation.TargetInventoryId = Line.TargetInventoryId;
        Operation.SourceSlot = Line.SourceSlot;
        Operation.TargetSlot = Line.TargetSlot;
        Operation.SourceGroups = Line.SourceGroups;
        Operation.TargetGroups = Line.TargetGroups;
        Operation.Filter = Line.Filter;
        Operation.Requested = Line.Requested;
        Operation.bAutomatic = Line.bAutomatic;
        TArray<int32> Candidates;
        EInventoryResultCode Code = GetCandidates(SourceIndex, Operation, false, Candidates);
        if (Code != EInventoryResultCode::Success)
        {
            return Code;
        }
        TArray<int32> TargetCandidates;
        Code = GetCandidates(TargetIndex, Operation, true, TargetCandidates);
        if (Code != EInventoryResultCode::Success)
        {
            return Code;
        }
        int32 Remaining = Line.Requested;
        for (const int32 SlotIndex : Candidates)
        {
            const FInventorySlotState Slot = Working[SourceIndex].Slots[SlotIndex];
            if (!Line.Filter.Matches(Slot.Item))
            {
                continue;
            }
            Code = CanTake(SourceIndex, SlotIndex, Operation);
            if (Code != EInventoryResultCode::Success)
            {
                if (Line.SourceSlot.IsValid())
                {
                    return Code;
                }
                continue;
            }
            const FGuid BoundOwner = Slot.Item.GetBoundOwner();
            if (BoundOwner.IsValid() && Working[TargetIndex].OwnerId != BoundOwner)
            {
                return EInventoryResultCode::ItemBound;
            }
            FReservedTransfer Transfer;
            Transfer.TargetIndex = TargetIndex;
            Transfer.Operation = Operation;
            Transfer.Item = Slot.Item;
            Transfer.Item.Count = FMath::Min(Remaining, Slot.Item.Count);
            Transfer.bFavorite = Slot.bFavorite;
            ReduceSlot(SourceIndex, SlotIndex, Transfer.Item.Count);
            Remaining -= Transfer.Item.Count;
            Transfers.Add(MoveTemp(Transfer));
            if (Remaining == 0)
            {
                break;
            }
        }
        if (Remaining != 0)
        {
            return EInventoryResultCode::InsufficientItems;
        }
    }
    for (const FReservedTransfer& Transfer : Transfers)
    {
        FAbilityItem ItemTemplate = Transfer.Item;
        ItemTemplate.Count = 1;
        int32 Accepted = 0;
        const EInventoryResultCode Code = Add(Transfer.TargetIndex, ItemTemplate, Transfer.Item.Count,
            EInventoryAmountMode::Exact, Transfer.Operation, Transfer.bFavorite, Accepted);
        if (Code != EInventoryResultCode::Success)
        {
            return Code;
        }
        OutResult.AcceptedCount += Accepted;
    }
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::Swap(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    const int32 SourceIndex = FindState(InOperation.SourceInventoryId);
    const int32 TargetIndex = FindState(InOperation.TargetInventoryId);
    if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InventoryGone;
    }
    const int32 SourceSlotIndex = FindSlot(SourceIndex, InOperation.SourceSlot);
    const int32 TargetSlotIndex = FindSlot(TargetIndex, InOperation.TargetSlot);
    if (!InOperation.SourceSlot.IsValid() || !InOperation.TargetSlot.IsValid()
        || SourceSlotIndex == INDEX_NONE || TargetSlotIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InvalidSlot;
    }
    if (SourceIndex == TargetIndex && SourceSlotIndex == TargetSlotIndex)
    {
        return EInventoryResultCode::NoChange;
    }
    const FInventorySlotState Source = Working[SourceIndex].Slots[SourceSlotIndex];
    const FInventorySlotState Target = Working[TargetIndex].Slots[TargetSlotIndex];
    if (Source.bLocked || Target.bLocked)
    {
        return EInventoryResultCode::ItemLocked;
    }
    if (Source.Item.IsEmpty() && Target.Item.IsEmpty())
    {
        return EInventoryResultCode::NoChange;
    }
    if (!Source.Item.IsEmpty() && !InOperation.Filter.Matches(Source.Item))
    {
        return EInventoryResultCode::StackMismatch;
    }
    if (!Source.Item.IsEmpty())
    {
        EInventoryResultCode Code = CanTake(SourceIndex, SourceSlotIndex, InOperation);
        if (Code != EInventoryResultCode::Success)
        {
            return Code;
        }
        Code = CanPut(TargetIndex, TargetSlotIndex, Source.Item, InOperation);
        if (Code != EInventoryResultCode::Success)
        {
            return Code;
        }
    }
    if (!Target.Item.IsEmpty())
    {
        EInventoryResultCode Code = CanTake(TargetIndex, TargetSlotIndex, InOperation);
        if (Code != EInventoryResultCode::Success)
        {
            return Code;
        }
        Code = CanPut(SourceIndex, SourceSlotIndex, Target.Item, InOperation);
        if (Code != EInventoryResultCode::Success)
        {
            return Code;
        }
    }
    Working[SourceIndex].Slots[SourceSlotIndex].Item = Target.Item;
    Working[SourceIndex].Slots[SourceSlotIndex].bFavorite = Target.bFavorite;
    Working[TargetIndex].Slots[TargetSlotIndex].Item = Source.Item;
    Working[TargetIndex].Slots[TargetSlotIndex].bFavorite = Source.bFavorite;
    OutResult.AcceptedCount = Source.Item.Count;
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::SetFavorite(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    const int32 SourceIndex = FindState(InOperation.SourceInventoryId);
    if (SourceIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InventoryGone;
    }
    const int32 SlotIndex = FindSlot(SourceIndex, InOperation.SourceSlot);
    if (!InOperation.SourceSlot.IsValid() || SlotIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InvalidSlot;
    }
    FInventorySlotState& Slot = Working[SourceIndex].Slots[SlotIndex];
    if (Slot.bLocked)
    {
        return EInventoryResultCode::ItemLocked;
    }
    if (Slot.Item.IsEmpty() || !InOperation.Filter.Matches(Slot.Item))
    {
        return EInventoryResultCode::InvalidItem;
    }
    if (Slot.bFavorite == InOperation.bFavorite)
    {
        return EInventoryResultCode::NoChange;
    }
    Slot.bFavorite = InOperation.bFavorite;
    OutResult.AcceptedCount = 0;
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::Transform(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    const int32 SourceIndex = FindState(InOperation.SourceInventoryId);
    if (SourceIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InventoryGone;
    }
    const int32 SlotIndex = FindSlot(SourceIndex, InOperation.SourceSlot);
    if (!InOperation.SourceSlot.IsValid() || SlotIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InvalidSlot;
    }
    const FAbilityItem Before = Working[SourceIndex].Slots[SlotIndex].Item;
    if (Before.IsEmpty() || !InOperation.Filter.Matches(Before) || InOperation.Item.ItemId != Before.ItemId
        || InOperation.Item.Count != 1
        || (InOperation.Item.InstanceId.IsValid() && InOperation.Item.InstanceId != Before.InstanceId))
    {
        return EInventoryResultCode::InvalidItem;
    }
    EInventoryResultCode Code = CanTake(SourceIndex, SlotIndex, InOperation);
    if (Code != EInventoryResultCode::Success)
    {
        return Code;
    }
    FAbilityItem After = InOperation.Item;
    After.Count = Before.Count;
    After.InstanceId = Before.InstanceId;
    FString Error;
    if (!Context.Registry.ValidateItem(After, Error))
    {
        return EInventoryResultCode::InvalidItem;
    }
    Code = CanPut(SourceIndex, SlotIndex, After, InOperation);
    if (Code != EInventoryResultCode::Success)
    {
        return Code;
    }
    if (Before == After)
    {
        return EInventoryResultCode::NoChange;
    }
    TrackQuantity(Before, 0, Before.Count);
    TrackQuantity(After, After.Count, 0);
    Working[SourceIndex].Slots[SlotIndex].Item = MoveTemp(After);
    OutResult.AcceptedCount = Before.Count;
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::Sort(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    const int32 StateIndex = FindState(InOperation.SourceInventoryId);
    if (StateIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InventoryGone;
    }
    FInventoryOperation Operation = InOperation;
    Operation.bAutomatic = true;
    TArray<int32> Candidates;
    EInventoryResultCode Code = GetCandidates(StateIndex, Operation, false, Candidates);
    if (Code != EInventoryResultCode::Success)
    {
        return Code;
    }
    struct FSortPartition
    {
        FGameplayTag Group;
        FInventorySlotRule Rule;
        TArray<int32> Slots;
    };
    TArray<FSortPartition> Partitions;
    for (const int32 SlotIndex : Candidates)
    {
        const FInventorySlotDefinition& Definition = GetProfile(StateIndex).Slots[SlotIndex];
        const FInventorySlotState& Slot = Working[StateIndex].Slots[SlotIndex];
        if (!Definition.Rule.bAllowsSort || !Definition.Rule.bAllowsAutomatic || !Definition.Rule.bAllowsInsert
            || !Definition.Rule.bAllowsRemove || Slot.bLocked || Slot.bFavorite
            || (!Slot.Item.IsEmpty() && (!Operation.Filter.Matches(Slot.Item)
                || CanTake(StateIndex, SlotIndex, Operation) != EInventoryResultCode::Success)))
        {
            continue;
        }
        FSortPartition* Partition = Partitions.FindByPredicate([&Definition](const FSortPartition& InPartition)
        {
            return InPartition.Group == Definition.Key.Group && InPartition.Rule == Definition.Rule;
        });
        if (!Partition)
        {
            FSortPartition NewPartition;
            NewPartition.Group = Definition.Key.Group;
            NewPartition.Rule = Definition.Rule;
            Partitions.Add(MoveTemp(NewPartition));
            Partition = &Partitions.Last();
        }
        Partition->Slots.Add(SlotIndex);
    }
    struct FSortedStack
    {
        FAbilityItem Item;
        int64 Count = 0;
    };
    for (FSortPartition& Partition : Partitions)
    {
        Partition.Slots.Sort();
        TArray<FSortedStack> Stacks;
        for (const int32 SlotIndex : Partition.Slots)
        {
            const FAbilityItem Item = Working[StateIndex].Slots[SlotIndex].Item;
            if (!Item.IsEmpty())
            {
                FSortedStack* Existing = Stacks.FindByPredicate([&Item](const FSortedStack& InStack)
                {
                    return InStack.Item.CanStackWith(Item);
                });
                if (Existing)
                {
                    Existing->Count += Item.Count;
                }
                else
                {
                    FSortedStack Stack;
                    Stack.Item = Item;
                    Stack.Item.Count = 1;
                    Stack.Count = Item.Count;
                    Stacks.Add(MoveTemp(Stack));
                }
            }
            Working[StateIndex].Slots[SlotIndex].Item = FAbilityItem();
        }
        Stacks.Sort([](const FSortedStack& InLeft, const FSortedStack& InRight)
        {
            return InLeft.Item.Signature().Compare(InRight.Item.Signature()) < 0;
        });
        TSet<int32> AssignedSlots;
        for (const FSortedStack& Stack : Stacks)
        {
            int64 Remaining = Stack.Count;
            const FInventoryItemDefinition& Definition = *Context.Registry.Find(Stack.Item.ItemId);
            const int32 Capacity = Partition.Rule.GetCapacity(Definition);
            for (const int32 SlotIndex : Partition.Slots)
            {
                if (AssignedSlots.Contains(SlotIndex)
                    || CanPut(StateIndex, SlotIndex, Stack.Item, Operation) != EInventoryResultCode::Success)
                {
                    continue;
                }
                const int32 Quantity = static_cast<int32>(FMath::Min<int64>(Capacity, Remaining));
                FInventorySlotState& Slot = Working[StateIndex].Slots[SlotIndex];
                Slot.Item = Stack.Item;
                Slot.Item.Count = Quantity;
                AssignedSlots.Add(SlotIndex);
                Remaining -= Quantity;
                if (Remaining == 0)
                {
                    break;
                }
            }
            if (Remaining != 0)
            {
                return EInventoryResultCode::SlotRuleRejected;
            }
        }
    }
    OutResult.AcceptedCount = 0;
    return EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::QuickStack(const FInventoryOperation& InOperation, FInventoryOperationResult& OutResult)
{
    const int32 SourceIndex = FindState(InOperation.SourceInventoryId);
    if (SourceIndex == INDEX_NONE)
    {
        return EInventoryResultCode::InventoryGone;
    }
    FInventoryOperation Operation = InOperation;
    Operation.bAutomatic = true;
    Operation.SourceSlot = FInventorySlotKey();
    Operation.TargetSlot = FInventorySlotKey();
    TArray<FGuid> Targets = InOperation.TargetInventories;
    if (Targets.IsEmpty() && InOperation.TargetInventoryId.IsValid())
    {
        Targets.Add(InOperation.TargetInventoryId);
    }
    if (Targets.IsEmpty())
    {
        return EInventoryResultCode::InvalidRequest;
    }
    Targets.Sort([](const FGuid& InLeft, const FGuid& InRight)
    {
        return InventoryOrder::GuidLess(InLeft, InRight);
    });
    for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
    {
        if (Targets[TargetIndex] == InOperation.SourceInventoryId || FindState(Targets[TargetIndex]) == INDEX_NONE
            || (TargetIndex > 0 && Targets[TargetIndex - 1] == Targets[TargetIndex]))
        {
            return EInventoryResultCode::InvalidRequest;
        }
    }
    TArray<int32> Candidates;
    EInventoryResultCode Code = GetCandidates(SourceIndex, Operation, false, Candidates);
    if (Code != EInventoryResultCode::Success)
    {
        return Code;
    }
    int64 RequestedTotal = 0;
    for (const int32 SlotIndex : Candidates)
    {
        const FInventorySlotState InitialSlot = Working[SourceIndex].Slots[SlotIndex];
        if (!Operation.Filter.Matches(InitialSlot.Item) || InitialSlot.Item.InstanceId.IsValid()
            || CanTake(SourceIndex, SlotIndex, Operation) != EInventoryResultCode::Success)
        {
            continue;
        }
        RequestedTotal += InitialSlot.Item.Count;
        for (const FGuid& TargetId : Targets)
        {
            const FInventorySlotState CurrentSource = Working[SourceIndex].Slots[SlotIndex];
            if (CurrentSource.Item.IsEmpty())
            {
                break;
            }
            const int32 TargetIndex = FindState(TargetId);
            FInventoryOperation TransferOperation = Operation;
            TransferOperation.Type = EInventoryOperationType::Transfer;
            TransferOperation.SourceSlot = CurrentSource.Key;
            TransferOperation.TargetInventoryId = TargetId;
            TransferOperation.AmountMode = EInventoryAmountMode::UpTo;
            TransferOperation.Requested = CurrentSource.Item.Count;
            TransferOperation.Filter.ItemId = CurrentSource.Item.ItemId;
            TransferOperation.Filter.bMatchSignature = true;
            TransferOperation.Filter.Signature = CurrentSource.Item.Signature();
            TArray<int32> TargetCandidates;
            Code = GetCandidates(TargetIndex, TransferOperation, true, TargetCandidates);
            if (Code != EInventoryResultCode::Success)
            {
                return Code;
            }
            bool bHasMatchingStack = false;
            for (const int32 TargetSlotIndex : TargetCandidates)
            {
                const FInventorySlotState& TargetSlot = Working[TargetIndex].Slots[TargetSlotIndex];
                if (TargetSlot.Item.CanStackWith(CurrentSource.Item)
                    && CanPut(TargetIndex, TargetSlotIndex, CurrentSource.Item, TransferOperation) == EInventoryResultCode::Success)
                {
                    bHasMatchingStack = true;
                    break;
                }
            }
            if (!bHasMatchingStack)
            {
                continue;
            }
            FInventoryOperationResult Result;
            Code = Transfer(TransferOperation, Result);
            if (Code == EInventoryResultCode::Success || Code == EInventoryResultCode::Partial)
            {
                OutResult.AcceptedCount += Result.AcceptedCount;
            }
            else if (Code != EInventoryResultCode::NoCapacity && Code != EInventoryResultCode::ItemBound
                && Code != EInventoryResultCode::ItemLocked && Code != EInventoryResultCode::SlotRuleRejected)
            {
                return Code;
            }
        }
    }
    OutResult.RemainingCount = RequestedTotal - OutResult.AcceptedCount;
    if (OutResult.AcceptedCount == 0)
    {
        return EInventoryResultCode::NoChange;
    }
    return OutResult.RemainingCount > 0 ? EInventoryResultCode::Partial : EInventoryResultCode::Success;
}

EInventoryResultCode FInventoryPlanBuilder::Finalize(FInventoryPlan& OutPlan, FString& OutError)
{
    for (int32 StateIndex = 0; StateIndex < Working.Num(); ++StateIndex)
    {
        const FInventorySnapshot& State = Working[StateIndex];
        const FInventoryProfileDefinition& Profile = GetProfile(StateIndex);
        if (!Profile.ValidateState(State, Context.Registry, OutError))
        {
            return EInventoryResultCode::InvalidState;
        }
        const EInventoryResultCode Code = GetRules().ValidateFinalState(State, Profile, Context.Registry);
        if (Code != EInventoryResultCode::Success)
        {
            OutError = TEXT("库存最终布局被领域规则拒绝。");
            return Code;
        }
    }
    if (!FInventoryPlanner::ValidateUniqueInstances(Working, OutError))
    {
        return EInventoryResultCode::DuplicateInstance;
    }
    if (!FInventoryPlanner::ValidateConservation(Original, Working, Plan.QuantityDeltas, OutError))
    {
        return EInventoryResultCode::InvalidState;
    }
    for (int32 StateIndex = 0; StateIndex < Working.Num(); ++StateIndex)
    {
        const FInventorySnapshot& Before = Original[StateIndex];
        FInventorySnapshot& After = Working[StateIndex];
        if (Before.Slots == After.Slots)
        {
            continue;
        }
        if (Before.Revision == MAX_uint64)
        {
            OutError = TEXT("库存版本已达到上限，不能继续提交。");
            return EInventoryResultCode::InvalidState;
        }
        After.Revision = Before.Revision + 1;
        Plan.Before.Add(Before);
        Plan.After.Add(After);
        FInventoryRevision Revision;
        Revision.InventoryId = Before.InventoryId;
        Revision.Before = Before.Revision;
        Revision.After = After.Revision;
        Plan.Changes.Revisions.Add(Revision);
        for (int32 SlotIndex = 0; SlotIndex < Before.Slots.Num(); ++SlotIndex)
        {
            if (Before.Slots[SlotIndex] == After.Slots[SlotIndex])
            {
                continue;
            }
            FInventoryChangedSlot Change;
            Change.InventoryId = Before.InventoryId;
            Change.Key = Before.Slots[SlotIndex].Key;
            Change.Before = Before.Slots[SlotIndex];
            Change.After = After.Slots[SlotIndex];
            Plan.Changes.ChangedSlots.Add(MoveTemp(Change));
        }
    }
    Plan.QuantityDeltas.Sort([](const FInventoryQuantityDelta& InLeft, const FInventoryQuantityDelta& InRight)
    {
        return InLeft.Signature.Compare(InRight.Signature) < 0;
    });
    Plan.Code = Plan.After.IsEmpty() ? EInventoryResultCode::NoChange
        : (Plan.RemainingCount > 0 ? EInventoryResultCode::Partial : EInventoryResultCode::Success);
    OutPlan = MoveTemp(Plan);
    return OutPlan.Code;
}
