#pragma once

#include "AI/Base/AIAgentInterface.h"
#include "FSM/Base/FiniteStateBase.h"
#include "FSM/Components/FSMComponent.h"
#include "GameFramework/Pawn.h"
#include "WHFSMTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class UWHFSMTestState : public UFiniteStateBase
{
	GENERATED_BODY()

public:
	UWHFSMTestState();
	virtual bool OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;
	virtual bool OnPreLeave(UFiniteStateBase* InNextState) override;
	virtual void OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;
	virtual void OnLeave(UFiniteStateBase* InNextState) override;
	virtual void OnTermination() override;
	virtual void OnPresentationEnter(UFiniteStateBase* InLastState, const FFSMReplicatedState& InState) override;
	virtual void OnPresentationLeave(UFiniteStateBase* InNextState) override;

	int32 EnterCount = 0;
	int32 LeaveCount = 0;
	int32 TerminationCount = 0;
	int32 PresentationEnterCount = 0;
	int32 PresentationLeaveCount = 0;
	bool bPermitEnter = true;
	bool bPermitLeave = true;
	bool bTerminateDuringEnter = false;
	bool bRequestNestedSwitch = false;
	bool bNestedSwitchAccepted = false;
};

UCLASS(Transient, NotBlueprintable)
class UWHFSMTestSecondState : public UWHFSMTestState
{
	GENERATED_BODY()

public:
	UWHFSMTestSecondState();
};

UCLASS(Transient, NotBlueprintable)
class UWHFSMTestOtherDomainState : public UWHFSMTestState
{
	GENERATED_BODY()

public:
	UWHFSMTestOtherDomainState();
};

UCLASS(Transient, NotBlueprintable)
class UWHFSMTestComponent : public UFSMComponent
{
	GENERATED_BODY()

public:
	void ReceiveSnapshot(const FFSMReplicatedState& InState);
};

UCLASS(Transient, NotBlueprintable)
class AWHBlackboardTestPawn : public APawn, public IAIAgentInterface
{
	GENERATED_BODY()

public:
	AWHBlackboardTestPawn();
	virtual UBehaviorTree* GetBehaviorTreeAsset() const override;
	virtual AAIControllerBase* GetAIController() const override;
};
