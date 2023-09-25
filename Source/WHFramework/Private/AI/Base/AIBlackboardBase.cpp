// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIBlackboardBase.h"
#include "AI/Base/AIAgentInterface.h"
#include "Math/MathTypes.h"

void UAIBlackboardBase::PostLoad()
{
	Super::PostLoad();

	// BLACKBOARD_VALUE_GENERATE_BOOL(IsLostTarget);
	// BLACKBOARD_VALUE_GENERATE_VECTOR(TargetLocation);
	// BLACKBOARD_VALUE_GENERATE_OBJECT(TargetCharacter, APawn);
}

void UAIBlackboardBase::Initialize(UBlackboardComponent* InComponent, IAIAgentInterface* InAgent)
{
	Component = InComponent;
	Agent = InAgent;
}

void UAIBlackboardBase::Refresh()
{
	OnRefresh();
}

void UAIBlackboardBase::OnRefresh()
{
	
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
				SetIsLostTarget(false);
			}
		}
		else
		{
			SetTargetLocation(Vector_Empty);
		}
	}
	else if(InValueName.IsEqual(FName("TargetCharacter")))
	{
		SetIsLostTarget(false);
	}
}
