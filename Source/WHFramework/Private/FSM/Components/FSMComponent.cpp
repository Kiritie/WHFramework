#include "FSM/Components/FSMComponent.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "FSM/FSMModule.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UFSMComponent::UFSMComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	bAutoSwitchDefault = false;
	GroupName = NAME_None;
	DomainName = NAME_None;
	ExecutionPolicy = EFSMExecutionPolicy::Authority;
	DefaultState = nullptr;
	FinalState = nullptr;
	CurrentState = nullptr;
	PresentedState = nullptr;
	bSwitchingState = false;
	bInitialized = false;
	bTerminating = false;
	bTerminationRequested = false;
	bShowDebugMessage = false;
}

void UFSMComponent::BeginPlay()
{
	Super::BeginPlay();
	OnInitialize();
}

void UFSMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnTermination();
	Super::EndPlay(EndPlayReason);
}

void UFSMComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFSMComponent, ReplicatedState);
}

bool UFSMComponent::CanExecuteStateLogic() const
{
	if (ExecutionPolicy == EFSMExecutionPolicy::Local)
	{
		return true;
	}
	return ExecutionPolicy == EFSMExecutionPolicy::Authority && GetOwner() && GetOwner()->HasAuthority();
}

void UFSMComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	OnRefresh(DeltaTime);
}

void UFSMComponent::OnInitialize()
{
	if (bInitialized || bTerminating)
	{
		return;
	}
	StateMap.Reset();
	OrderedStates.Reset();
	for (TSubclassOf<UFiniteStateBase> StateClass : States)
	{
		if (!StateClass)
		{
			continue;
		}
		const UFiniteStateBase* Definition = StateClass->GetDefaultObject<UFiniteStateBase>();
		const FName StateName = Definition->GetStateName();
		if (StateName.IsNone() || Definition->GetStateDomain() != DomainName || StateMap.Contains(StateName))
		{
			UE_LOG(LogTemp, Error, TEXT("FSM状态配置无效：%s，域=%s，状态=%s"), *GetNameSafe(GetOwner()), *DomainName.ToString(), *StateName.ToString());
			continue;
		}
		UFiniteStateBase* State = NewObject<UFiniteStateBase>(this, StateClass);
		State->OnInitialize(this, OrderedStates.Num());
		StateMap.Add(StateName, State);
		OrderedStates.Add(State);
	}
	bInitialized = true;
	if (UFSMModule* Module = UFSMModule::GetPtr(GetWorld()))
	{
		Module->RegisterFSM(this);
	}
	if (CanExecuteStateLogic())
	{
		if (bAutoSwitchDefault)
		{
			SwitchDefaultState();
		}
	}
	else
	{
		OnRep_ReplicatedState();
	}
}

void UFSMComponent::OnRefresh(float DeltaSeconds)
{
	if (bInitialized && !bTerminating && CanExecuteStateLogic() && CurrentState)
	{
		CurrentState->OnRefresh(DeltaSeconds);
	}
}

void UFSMComponent::OnTermination()
{
	if (!bInitialized || bTerminating)
	{
		return;
	}
	if (bSwitchingState)
	{
		bTerminationRequested = true;
		return;
	}
	TGuardValue<bool> TerminatingGuard(bTerminating, true);
	bTerminationRequested = false;
	UFiniteStateBase* LastState = CurrentState;
	CurrentState = nullptr;
	if (CanExecuteStateLogic() && LastState)
	{
		LastState->OnLeave(nullptr);
	}
	if (PresentedState)
	{
		PresentedState->OnPresentationLeave(nullptr);
		PresentedState = nullptr;
	}
	if (UFSMModule* Module = UFSMModule::GetPtr(GetWorld()))
	{
		Module->UnregisterFSM(this);
	}
	for (UFiniteStateBase* State : OrderedStates)
	{
		State->OnTermination();
	}
	StateMap.Reset();
	OrderedStates.Reset();
	bInitialized = false;
	PresentedRevision = INDEX_NONE;
	ReplicatedState.State = NAME_None;
	++ReplicatedState.Revision;
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
}

double UFSMComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void UFSMComponent::UpdateReplicatedState()
{
	ReplicatedState.Domain = DomainName;
	ReplicatedState.State = CurrentState ? CurrentState->GetStateName() : NAME_None;
	ReplicatedState.StartedAt = GetServerTime();
	++ReplicatedState.Revision;
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
}

void UFSMComponent::ApplyPresentation(UFiniteStateBase* InState)
{
	if ((GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer) || (PresentedState == InState && PresentedRevision == ReplicatedState.Revision))
	{
		return;
	}
	UFiniteStateBase* LastState = PresentedState;
	PresentedState = nullptr;
	if (LastState)
	{
		LastState->OnPresentationLeave(InState);
	}
	PresentedState = InState;
	PresentedRevision = ReplicatedState.Revision;
	if (PresentedState)
	{
		PresentedState->OnPresentationEnter(LastState, ReplicatedState);
	}
	OnStatePresented.Broadcast(InState, LastState);
}

void UFSMComponent::OnRep_ReplicatedState()
{
	if (!bInitialized || CanExecuteStateLogic() || ReplicatedState.Domain != DomainName)
	{
		return;
	}
	UFiniteStateBase* NewState = ReplicatedState.State.IsNone() ? nullptr : GetStateByName(ReplicatedState.State, nullptr);
	if (!ReplicatedState.State.IsNone() && !NewState)
	{
		UE_LOG(LogTemp, Error, TEXT("FSM复制状态未定义：%s"), *ReplicatedState.State.ToString());
		return;
	}
	CurrentState = NewState;
	ApplyPresentation(NewState);
}

bool UFSMComponent::SwitchState(UFiniteStateBase* InState, const TArray<FParameter>& InParams)
{
	if (!bInitialized || bTerminating || bSwitchingState || !CanExecuteStateLogic() || InState == CurrentState ||
		(InState && (!HasState(InState) || InState->GetStateDomain() != DomainName)))
	{
		return false;
	}
	UFiniteStateBase* LastState = CurrentState;
	bool bAccepted = true;
	{
		TGuardValue<bool> SwitchingGuard(bSwitchingState, true);
		bAccepted = (!LastState || LastState->OnPreLeave(InState)) && (!InState || InState->OnPreEnter(LastState, InParams));
		if (bAccepted)
		{
			CurrentState = nullptr;
			if (LastState)
			{
				LastState->OnLeave(InState);
			}
			if (!bTerminationRequested)
			{
				CurrentState = InState;
				if (CurrentState)
				{
					CurrentState->OnEnter(LastState, InParams);
				}
				UpdateReplicatedState();
				ApplyPresentation(CurrentState);
				if (IFSMAgentInterface* Agent = Cast<IFSMAgentInterface>(GetOwner()))
				{
					Agent->OnFiniteStateChanged(CurrentState, LastState);
				}
				OnStateChanged.Broadcast(CurrentState, LastState);
			}
		}
	}
	if (bTerminationRequested)
	{
		OnTermination();
	}
	else if (bAccepted && !CurrentState)
	{
		RefreshState();
	}
	return bAccepted;
}

bool UFSMComponent::SwitchStateByIndex(int32 InStateIndex, const TArray<FParameter>& InParams)
{
	return OrderedStates.IsValidIndex(InStateIndex) && SwitchState(OrderedStates[InStateIndex], InParams);
}

bool UFSMComponent::SwitchStateByName(FName InStateName, const TArray<FParameter>& InParams)
{
	UFiniteStateBase* State = GetStateByName(InStateName, nullptr);
	return State && SwitchState(State, InParams);
}

bool UFSMComponent::SwitchStateByClass(TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams)
{
	UFiniteStateBase* State = GetStateByClass(InStateClass);
	return State && SwitchState(State, InParams);
}

bool UFSMComponent::SwitchDefaultState(const TArray<FParameter>& InParams)
{
	return DefaultState && SwitchStateByClass(DefaultState, InParams);
}

bool UFSMComponent::SwitchFinalState(const TArray<FParameter>& InParams)
{
	return FinalState && SwitchStateByClass(FinalState, InParams);
}

bool UFSMComponent::SwitchLastState(const TArray<FParameter>& InParams)
{
	return CurrentState && SwitchStateByIndex(CurrentState->GetStateIndex() - 1, InParams);
}

bool UFSMComponent::SwitchNextState(const TArray<FParameter>& InParams)
{
	return CurrentState && SwitchStateByIndex(CurrentState->GetStateIndex() + 1, InParams);
}

void UFSMComponent::RefreshState()
{
	if (!bInitialized || bTerminating || bSwitchingState || !CanExecuteStateLogic())
	{
		return;
	}
	if (IFSMAgentInterface* Agent = Cast<IFSMAgentInterface>(GetOwner()))
	{
		Agent->OnFiniteStateRefresh(CurrentState);
	}
}

bool UFSMComponent::TerminateState(UFiniteStateBase* InState)
{
	if (!bInitialized || bTerminating || bSwitchingState || !CanExecuteStateLogic() || !HasState(InState))
	{
		return false;
	}
	const bool bWasCurrent = CurrentState == InState;
	{
		TGuardValue<bool> SwitchingGuard(bSwitchingState, true);
		if (bWasCurrent)
		{
			CurrentState = nullptr;
			InState->OnLeave(nullptr);
			UpdateReplicatedState();
			ApplyPresentation(nullptr);
			if (IFSMAgentInterface* Agent = Cast<IFSMAgentInterface>(GetOwner()))
			{
				Agent->OnFiniteStateChanged(nullptr, InState);
			}
			OnStateChanged.Broadcast(nullptr, InState);
		}
		StateMap.Remove(InState->GetStateName());
		OrderedStates.RemoveSingle(InState);
		InState->OnTermination();
		for (int32 Index = 0; Index < OrderedStates.Num(); ++Index)
		{
			OrderedStates[Index]->SetStateIndex(Index);
		}
	}
	if (bTerminationRequested)
	{
		OnTermination();
	}
	else if (bWasCurrent)
	{
		RefreshState();
	}
	return true;
}

bool UFSMComponent::HasState(UFiniteStateBase* InState) const
{
	return InState && StateMap.FindRef(InState->GetStateName()) == InState;
}

bool UFSMComponent::HasStateByIndex(int32 InStateIndex) const
{
	return OrderedStates.IsValidIndex(InStateIndex);
}

UFiniteStateBase* UFSMComponent::GetStateByIndex(int32 InStateIndex, TSubclassOf<UFiniteStateBase> InStateClass) const
{
	UFiniteStateBase* State = OrderedStates.IsValidIndex(InStateIndex) ? OrderedStates[InStateIndex] : nullptr;
	return State && (!InStateClass || State->IsA(InStateClass)) ? State : nullptr;
}

bool UFSMComponent::HasStateByName(FName InStateName) const
{
	return !InStateName.IsNone() && StateMap.Contains(InStateName);
}

UFiniteStateBase* UFSMComponent::GetStateByName(FName InStateName, TSubclassOf<UFiniteStateBase> InStateClass) const
{
	UFiniteStateBase* State = StateMap.FindRef(InStateName);
	return State && (!InStateClass || State->IsA(InStateClass)) ? State : nullptr;
}

bool UFSMComponent::HasStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return GetStateByClass(InStateClass) != nullptr;
}

UFiniteStateBase* UFSMComponent::GetStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	if (!InStateClass)
	{
		return nullptr;
	}
	const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->GetStateName();
	return GetStateByName(StateName, InStateClass);
}

bool UFSMComponent::IsCurrentState(UFiniteStateBase* InState) const
{
	return CurrentState == InState;
}

bool UFSMComponent::IsCurrentStateIndex(int32 InStateIndex) const
{
	return CurrentState && CurrentState->GetStateIndex() == InStateIndex;
}

bool UFSMComponent::IsCurrentStateClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return CurrentState && InStateClass && CurrentState->IsA(InStateClass);
}

UFiniteStateBase* UFSMComponent::GetCurrentState(TSubclassOf<UFiniteStateBase> InClass) const
{
	return CurrentState && (!InClass || CurrentState->IsA(InClass)) ? CurrentState : nullptr;
}

TArray<UFiniteStateBase*> UFSMComponent::GetStates() const
{
	return OrderedStates;
}

AActor* UFSMComponent::GetAgent(TSubclassOf<AActor> InClass) const
{
	AActor* Owner = GetOwner();
	return Owner && (!InClass || Owner->IsA(InClass)) ? Owner : nullptr;
}
