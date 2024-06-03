// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIBlackboardBase.h"

#include "AI/Base/AIControllerBase.h"
#include "Math/MathTypes.h"

void UAIBlackboardBase::PostLoad()
{
	Super::PostLoad();

	// BLACKBOARD_VALUE_GENERATE_BOOL(IsLostTarget);
	// BLACKBOARD_VALUE_GENERATE_VECTOR(TargetLocation);
	// BLACKBOARD_VALUE_GENERATE_OBJECT(TargetAgent, APawn);
}

void UAIBlackboardBase::OnInitialize()
{
	Reset();
}

void UAIBlackboardBase::OnReset()
{
	ResetIsLostTarget();
	ResetTargetLocation();
	ResetTargetAgent();
}

void UAIBlackboardBase::OnRefresh()
{
	
}

void UAIBlackboardBase::OnValueReset(FName InValueName)
{
	if(InValueName.IsEqual(FName("IsLostTarget")))
	{
		SetIsLostTarget(false);
	}
	else if(InValueName.IsEqual(FName("TargetLocation")))
	{
		SetTargetLocation(EMPTY_Vector);
	}
	else if(InValueName.IsEqual(FName("TargetAgent")))
	{
		SetTargetAgent(nullptr);
	}
}

void UAIBlackboardBase::OnValuePreChange(FName InValueName)
{
	OnBlackboardValuePreChange.Broadcast(InValueName);
}

void UAIBlackboardBase::OnValueChanged(FName InValueName)
{
	OnBlackboardValueChanged.Broadcast(InValueName);

	if(InValueName.IsEqual(FName("IsLostTarget")))
	{
		if(GetIsLostTarget())
		{
			if(GetTargetAgent<APawn>())
			{
				SetTargetLocation(GetTargetAgent<APawn>()->GetActorLocation());
			}
			else
			{
				ResetIsLostTarget();
			}
		}
		else
		{
			ResetTargetLocation();
		}
	}
}

void UAIBlackboardBase::Initialize(UBlackboardComponent* InComponent)
{
	Component = InComponent;
	Controller = Cast<AAIControllerBase>(Component->GetOwner());
	if (Controller)
	{
		Agent = Cast<IAIAgentInterface>(Controller->GetPawn());
	}

	OnInitialize();
}

void UAIBlackboardBase::Reset()
{
	OnReset();
}

void UAIBlackboardBase::ResetValue(FName InValueName)
{
	OnValueReset(InValueName);
}

void UAIBlackboardBase::Refresh()
{
	OnRefresh();
}
