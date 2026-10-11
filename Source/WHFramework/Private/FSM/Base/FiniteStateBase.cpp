#include "FSM/Base/FiniteStateBase.h"
#include "Common/CommonModuleStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/FSM/Event_FiniteStateEntered.h"
#include "Event/Events/FSM/Event_FiniteStateLeaved.h"
#include "FSM/Components/FSMComponent.h"

UFiniteStateBase::UFiniteStateBase()
{
	StateName = TEXT("FiniteStateBase");
	StateDomain = NAME_None;
	StateIndex = INDEX_NONE;
	FSM = nullptr;
	bHasBlueprintOnPreEnter = false;
	bHasBlueprintOnPreLeave = false;
}

void UFiniteStateBase::OnSpawn_Implementation(const FParameter& InParam)
{
}

void UFiniteStateBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	StateIndex = INDEX_NONE;
	FSM = nullptr;
}

void UFiniteStateBase::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	FSM = InFSM;
	StateIndex = InStateIndex;
	bHasBlueprintOnPreEnter = UCommonModuleStatics::IsImplementedInBlueprint(GetClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UFiniteStateBase, K2_OnPreEnter)));
	bHasBlueprintOnPreLeave = UCommonModuleStatics::IsImplementedInBlueprint(GetClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UFiniteStateBase, K2_OnPreLeave)));
	K2_OnInitialize();
}

bool UFiniteStateBase::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return !bHasBlueprintOnPreEnter || K2_OnPreEnter(InLastState, InParams);
}

void UFiniteStateBase::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	K2_OnEnter(InLastState, InParams);
	UEventModuleStatics::BroadcastEvent<FEventFiniteStateEntered>(this, {this, FSM});
}

void UFiniteStateBase::OnRefresh(float DeltaSeconds)
{
	K2_OnRefresh(DeltaSeconds);
}

bool UFiniteStateBase::OnPreLeave(UFiniteStateBase* InNextState)
{
	return !bHasBlueprintOnPreLeave || K2_OnPreLeave(InNextState);
}

void UFiniteStateBase::OnLeave(UFiniteStateBase* InNextState)
{
	K2_OnLeave(InNextState);
	UEventModuleStatics::BroadcastEvent<FEventFiniteStateLeaved>(this, {this, FSM});
}

void UFiniteStateBase::OnPresentationEnter(UFiniteStateBase* InLastState, const FFSMReplicatedState& InState)
{
	K2_OnPresentationEnter(InLastState, InState);
}

void UFiniteStateBase::OnPresentationLeave(UFiniteStateBase* InNextState)
{
	K2_OnPresentationLeave(InNextState);
}

void UFiniteStateBase::OnTermination()
{
	K2_OnTermination();
	FSM = nullptr;
	StateIndex = INDEX_NONE;
}

void UFiniteStateBase::Terminate()
{
	if (FSM)
	{
		FSM->TerminateState(this);
	}
}

bool UFiniteStateBase::Switch(UFiniteStateBase* InState, const TArray<FParameter>& InParams)
{
	return FSM && FSM->SwitchState(InState, InParams);
}

bool UFiniteStateBase::SwitchLast(const TArray<FParameter>& InParams)
{
	return FSM && FSM->SwitchLastState(InParams);
}

bool UFiniteStateBase::SwitchNext(const TArray<FParameter>& InParams)
{
	return FSM && FSM->SwitchNextState(InParams);
}

AActor* UFiniteStateBase::GetAgent(TSubclassOf<AActor> InClass) const
{
	return FSM ? FSM->GetAgent(InClass) : nullptr;
}

bool UFiniteStateBase::IsCurrent()
{
	return FSM && FSM->IsCurrentState(this);
}
