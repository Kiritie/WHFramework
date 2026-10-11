#include "Tests/WHFSMTestTypes.h"
#include "AI/Base/AIControllerBase.h"

UWHFSMTestState::UWHFSMTestState()
{
	StateName = TEXT("Test.First");
	StateDomain = TEXT("Test.Domain");
}

bool UWHFSMTestState::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return bPermitEnter;
}

bool UWHFSMTestState::OnPreLeave(UFiniteStateBase* InNextState)
{
	return bPermitLeave;
}

void UWHFSMTestState::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	++EnterCount;
	if (bRequestNestedSwitch)
	{
		bNestedSwitchAccepted = FSM->SwitchStateByClass<UWHFSMTestSecondState>();
	}
	if (bTerminateDuringEnter)
	{
		FSM->OnTermination();
	}
}

void UWHFSMTestState::OnLeave(UFiniteStateBase* InNextState)
{
	++LeaveCount;
}

void UWHFSMTestState::OnTermination()
{
	++TerminationCount;
	Super::OnTermination();
}

void UWHFSMTestState::OnPresentationEnter(UFiniteStateBase* InLastState, const FFSMReplicatedState& InState)
{
	++PresentationEnterCount;
}

void UWHFSMTestState::OnPresentationLeave(UFiniteStateBase* InNextState)
{
	++PresentationLeaveCount;
}

UWHFSMTestSecondState::UWHFSMTestSecondState()
{
	StateName = TEXT("Test.Second");
}

UWHFSMTestOtherDomainState::UWHFSMTestOtherDomainState()
{
	StateName = TEXT("Test.Other");
	StateDomain = TEXT("Other.Domain");
}

void UWHFSMTestComponent::ReceiveSnapshot(const FFSMReplicatedState& InState)
{
	ReplicatedState = InState;
	OnRep_ReplicatedState();
}

AWHBlackboardTestPawn::AWHBlackboardTestPawn()
{
	AutoPossessAI = EAutoPossessAI::Disabled;
}

UBehaviorTree* AWHBlackboardTestPawn::GetBehaviorTreeAsset() const
{
	return nullptr;
}

AAIControllerBase* AWHBlackboardTestPawn::GetAIController() const
{
	return Cast<AAIControllerBase>(GetController());
}
