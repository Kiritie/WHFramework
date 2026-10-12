#include "Procedure/ProcedureModule.h"

#include "Event/EventModuleStatics.h"
#include "Event/Events/Procedure/Event_SwitchProcedure.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"
#include "Templates/UnrealTemplate.h"

IMPLEMENTATION_MODULE(UProcedureModule)

UProcedureModule::UProcedureModule()
{
    ModuleName = TEXT("ProcedureModule");
    ModuleDisplayName = NSLOCTEXT("Procedure", "Module", "流程模块");
    ModuleNetworkComponent = UProcedureModuleNetworkComponent::StaticClass();
}

UProcedureModule::~UProcedureModule()
{
    TERMINATION_MODULE(UProcedureModule)
}

#if WITH_EDITOR
void UProcedureModule::OnGenerate()
{
    Super::OnGenerate();
}

void UProcedureModule::OnDestroy()
{
    Super::OnDestroy();
    TERMINATION_MODULE(UProcedureModule)
}

void UProcedureModule::SetDefaultAsset(UProcedureAsset* InAsset)
{
    DefaultAsset = InAsset;
}
#endif

void UProcedureModule::OnInitialize()
{
    Super::OnInitialize();
    UEventModuleStatics::SubscribeEvent<FEventSwitchProcedure>(this, &ThisClass::OnSwitchProcedure);
}

void UProcedureModule::OnPreparatory(EPhase InPhase)
{
    Super::OnPreparatory(InPhase);
    if (PHASEC(InPhase, EPhase::Final) && DefaultAsset && DefaultAsset->bAutoStart)
    {
        StartMain();
    }
}

void UProcedureModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
    Super::OnRefresh(DeltaSeconds, bInEditor);
    if (bInEditor || bStopping)
    {
        return;
    }
    if (bStopRequested)
    {
        StopMain();
        return;
    }
    if (bHasQueuedSwitch)
    {
        const FProcedureActivationToken Token = QueuedToken;
        UProcedureBase* Target = QueuedTarget.IsValid() ? GetProcedureById(QueuedTarget) : nullptr;
        bHasQueuedSwitch = false;
        CommitSwitch(Token, Target);
    }
    if (CurrentProcedure)
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        CurrentProcedure->OnRefresh();
    }
    if (bStopRequested)
    {
        StopMain();
    }
}

void UProcedureModule::OnPause()
{
    Super::OnPause();
}

void UProcedureModule::OnUnPause()
{
    Super::OnUnPause();
}

void UProcedureModule::OnTermination(EPhase InPhase)
{
    if (PHASEC(InPhase, EPhase::Primary))
    {
        UEventModuleStatics::UnsubscribeEvent<FEventSwitchProcedure>(this);
        StopMain();
    }
    Super::OnTermination(InPhase);
}

FString UProcedureModule::GetModuleDebugMessage()
{
    return FString::Printf(TEXT("CurrentProcedure: %s; reachable: %d; activation: %lld"),
        *GetNameSafe(CurrentProcedure), Instances.Num(), ActivationSerial);
}

bool UProcedureModule::StartMain()
{
    if (bDispatching || bStopping || CurrentAsset || !DefaultAsset)
    {
        return false;
    }
    FString Error;
    if (!DefaultAsset->ValidateCompiled(Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to start Procedure: %s"), *Error);
        return false;
    }
    if (!DefaultAsset->EntryNodeId.IsValid())
    {
        return false;
    }
    CurrentAsset = DefaultAsset;
    for (UProcedureBase* Definition : CurrentAsset->CompiledNodes)
    {
        UProcedureBase* RuntimeProcedure = DuplicateObject<UProcedureBase>(Definition, this);
        RuntimeProcedure->ClearFlags(RF_Public | RF_Standalone | RF_Transactional);
        RuntimeProcedure->SetFlags(RF_Transient);
        RuntimeProcedure->RuntimeOwner = this;
        RuntimeProcedure->ProcedureState = EProcedureState::None;
        RuntimeProcedure->Activation = FProcedureActivationToken();
        Instances.Add(RuntimeProcedure);
        InstanceMap.Add(RuntimeProcedure->NodeId, RuntimeProcedure);
    }
    for (const FProcedureLink& Link : CurrentAsset->CompiledLinks)
    {
        NextIds.Add(Link.From, Link.To);
        PreviousIds.Add(Link.To, Link.From);
    }
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        for (UProcedureBase* RuntimeProcedure : Instances)
        {
            ++InitializedInstanceCount;
            RuntimeProcedure->OnInitialize();
            if (bStopRequested)
            {
                break;
            }
        }
        bMainStarted = true;
        if (!bStopRequested)
        {
            Enter(GetProcedureById(CurrentAsset->EntryNodeId));
        }
    }
    if (bStopRequested)
    {
        StopMain();
    }
    return CurrentProcedure != nullptr;
}

void UProcedureModule::StopMain()
{
    if (bStopping)
    {
        return;
    }
    if (bDispatching)
    {
        bStopRequested = true;
        bHasQueuedSwitch = false;
        return;
    }
    bStopRequested = false;
    TGuardValue<bool> StoppingGuard(bStopping, true);
    TGuardValue<bool> DispatchGuard(bDispatching, true);
    bHasQueuedSwitch = false;
    UProcedureBase* Previous = CurrentProcedure;
    CurrentProcedure = nullptr;
    if (Previous)
    {
        Previous->OnLeave(nullptr);
    }
    for (int32 Index = 0; Index < Instances.Num(); ++Index)
    {
        UProcedureBase* RuntimeProcedure = Instances[Index];
        if (Index < InitializedInstanceCount)
        {
            RuntimeProcedure->OnShutdown();
        }
        RuntimeProcedure->RuntimeOwner = nullptr;
        RuntimeProcedure->Activation = FProcedureActivationToken();
    }
    NextIds.Reset();
    PreviousIds.Reset();
    bMainStarted = false;
    InstanceMap.Reset();
    Instances.Reset();
    InitializedInstanceCount = 0;
    CurrentAsset = nullptr;
}


FProcedureActivationToken UProcedureModule::CaptureActivation() const
{
    FProcedureActivationToken Token;
    Token.NodeId = CurrentProcedure ? CurrentProcedure->NodeId : FGuid();
    Token.Serial = ActivationSerial;
    return Token;
}

bool UProcedureModule::CanSwitchProcedure(UProcedureBase* InProcedure) const
{
    if (!bMainStarted || bStopping || bStopRequested || bLeaving || bHasQueuedSwitch || InProcedure == CurrentProcedure)
    {
        return false;
    }
    return !InProcedure || (InProcedure->GetProcedureModule() == this && GetProcedureById(InProcedure->NodeId) == InProcedure);
}

bool UProcedureModule::SwitchProcedure(UProcedureBase* InProcedure)
{
    return SwitchProcedureFrom(CaptureActivation(), InProcedure);
}

bool UProcedureModule::SwitchProcedureFrom(const FProcedureActivationToken& Token, UProcedureBase* InProcedure)
{
    if (!(CaptureActivation() == Token) || !CanSwitchProcedure(InProcedure))
    {
        return false;
    }
    if (bDispatching)
    {
        bHasQueuedSwitch = true;
        QueuedToken = Token;
        QueuedTarget = InProcedure ? InProcedure->NodeId : FGuid();
        return true;
    }
    return CommitSwitch(Token, InProcedure);
}

bool UProcedureModule::CommitSwitch(const FProcedureActivationToken& Token, UProcedureBase* Target)
{
    if (bDispatching || !(CaptureActivation() == Token) || !CanSwitchProcedure(Target))
    {
        return false;
    }
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        Enter(Target);
    }
    if (bStopRequested)
    {
        StopMain();
    }
    return true;
}

bool UProcedureModule::CanSwitchProcedureByIndex(int32 InIndex) const
{
    return HasProcedureByIndex(InIndex) && CanSwitchProcedure(GetProcedureByIndex(InIndex));
}

bool UProcedureModule::SwitchProcedureByIndex(int32 InIndex)
{
    return HasProcedureByIndex(InIndex) && SwitchProcedure(GetProcedureByIndex(InIndex));
}

bool UProcedureModule::CanSwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass) const
{
    UProcedureBase* Target = GetProcedureByClass(InClass);
    return Target && CanSwitchProcedure(Target);
}

bool UProcedureModule::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
    if (!InClass)
    {
        return false;
    }
    UProcedureBase* Target = GetProcedureByClass(InClass);
    return Target && SwitchProcedure(Target);
}

UProcedureBase* UProcedureModule::GetNextProcedure(FName Output) const
{
    if (!CurrentProcedure || Output != TEXT("Out"))
    {
        return nullptr;
    }
    const FGuid* Target = NextIds.Find(CurrentProcedure->NodeId);
    return Target ? GetProcedureById(*Target) : nullptr;
}

UProcedureBase* UProcedureModule::GetPreviousProcedure() const
{
    const FGuid* Target = CurrentProcedure ? PreviousIds.Find(CurrentProcedure->NodeId) : nullptr;
    return Target ? GetProcedureById(*Target) : nullptr;
}

bool UProcedureModule::CanAdvance(FName Output) const
{
    UProcedureBase* Target = GetNextProcedure(Output);
    return Target && CanSwitchProcedure(Target);
}

bool UProcedureModule::Advance(FName Output)
{
    return AdvanceFrom(CaptureActivation(), Output);
}

bool UProcedureModule::AdvanceFrom(const FProcedureActivationToken& Token, FName Output)
{
    UProcedureBase* Target = GetNextProcedure(Output);
    return Target && SwitchProcedureFrom(Token, Target);
}

bool UProcedureModule::SwitchFirstProcedure()
{
    UProcedureBase* First = GetFirstProcedure();
    return First && SwitchProcedure(First);
}

bool UProcedureModule::SwitchLastProcedure()
{
    UProcedureBase* Previous = GetPreviousProcedure();
    return Previous && SwitchProcedure(Previous);
}

bool UProcedureModule::SwitchPreProcedure()
{
    return SwitchLastProcedure();
}

bool UProcedureModule::SwitchNextProcedure()
{
    return Advance();
}

bool UProcedureModule::HasProcedureByIndex(int32 InIndex) const
{
    return Instances.IsValidIndex(InIndex);
}

UProcedureBase* UProcedureModule::GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass) const
{
    UProcedureBase* Target = HasProcedureByIndex(InIndex) ? Instances[InIndex].Get() : nullptr;
    return Target && (!InClass || Target->IsA(InClass)) ? Target : nullptr;
}

bool UProcedureModule::IsCurrentProcedureIndex(int32 InIndex) const
{
    return HasProcedureByIndex(InIndex) && CurrentProcedure == Instances[InIndex];
}

UProcedureBase* UProcedureModule::GetFirstProcedure() const
{
    return GetProcedureByIndex(0);
}

void UProcedureModule::Enter(UProcedureBase* Next)
{
    UProcedureBase* Previous = CurrentProcedure;
    CurrentProcedure = nullptr;
    if (Previous)
    {
        TGuardValue<bool> LeavingGuard(bLeaving, true);
        Previous->OnLeave(Next);
    }
    CurrentProcedure = bStopRequested ? nullptr : Next;
    ++ActivationSerial;
    if (CurrentProcedure)
    {
        Next->Activation.NodeId = Next->NodeId;
        Next->Activation.Serial = ActivationSerial;
        Next->OnEnter(Previous);
    }
}


void UProcedureModule::OnSwitchProcedure(UObject* InSender, const FEventSwitchProcedure& InEvent)
{
    UProcedureBase* Target = GetProcedureByClass(InEvent.ProcedureClass);
    if (!Target)
    {
        return;
    }
    if (InEvent.Token.Serial != 0)
    {
        SwitchProcedureFrom(InEvent.Token, Target);
    }
    else
    {
        SwitchProcedure(Target);
    }
}

UProcedureBase* UProcedureModule::GetProcedureById(FGuid InNodeId) const
{
    const TObjectPtr<UProcedureBase>* Found = InstanceMap.Find(InNodeId);
    return Found ? Found->Get() : nullptr;
}

UProcedureBase* UProcedureModule::GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass) const
{
    return CurrentProcedure && (!InClass || CurrentProcedure->IsA(InClass)) ? CurrentProcedure.Get() : nullptr;
}

TArray<UProcedureBase*> UProcedureModule::GetProcedures() const
{
    TArray<UProcedureBase*> Result;
    for (UProcedureBase* RuntimeProcedure : Instances)
    {
        Result.Add(RuntimeProcedure);
    }
    return Result;
}

UProcedureBase* UProcedureModule::GetProcedureByClass(TSubclassOf<UProcedureBase> InClass) const
{
    UProcedureBase* Result = nullptr;
    if (!InClass)
    {
        return nullptr;
    }
    for (UProcedureBase* RuntimeProcedure : Instances)
    {
        if (RuntimeProcedure->IsA(InClass))
        {
            if (Result)
            {
                return nullptr;
            }
            Result = RuntimeProcedure;
        }
    }
    return Result;
}

bool UProcedureModule::HasProcedureByClass(TSubclassOf<UProcedureBase> InClass) const
{
    return GetProcedureByClass(InClass) != nullptr;
}

bool UProcedureModule::IsCurrentProcedure(const UProcedureBase* InProcedure) const
{
    return InProcedure && InProcedure == CurrentProcedure;
}

bool UProcedureModule::IsCurrentProcedureClass(TSubclassOf<UProcedureBase> InClass) const
{
    return CurrentProcedure && InClass && CurrentProcedure->IsA(InClass);
}

void UProcedureModule::GuideCurrentProcedure()
{
    if (CurrentProcedure)
    {
        TGuardValue<bool> DispatchGuard(bDispatching, true);
        CurrentProcedure->Guide();
    }
    if (bStopRequested)
    {
        StopMain();
    }
}
