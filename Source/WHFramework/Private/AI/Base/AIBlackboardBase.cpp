// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIBlackboardBase.h"

#include "Math/MathTypes.h"

class UBlackboardKeyType_Class;

void UAIBlackboardBase::PostLoad()
{
	Super::PostLoad();

	// BLACKBOARD_VALUE_GENERATE_BOOL(IsLostTarget);
	// BLACKBOARD_VALUE_GENERATE_VECTOR(TargetLocation);
	// BLACKBOARD_VALUE_GENERATE_OBJECT(TargetCharacter, ADWCharacter);
}

void UAIBlackboardBase::OnInitialize(UBlackboardComponent* InComponent, ACharacterBase* InOwnerCharacter)
{
	Component = InComponent;
	Character = InOwnerCharacter;
	OnBlackboardValuePreChange.AddUObject(this, &UAIBlackboardBase::OnValuePreChange);
	OnBlackboardValueChanged.AddUObject(this, &UAIBlackboardBase::OnValueChanged);
	OnRefresh();
}

void UAIBlackboardBase::OnRefresh()
{
	
}

void UAIBlackboardBase::OnValuePreChange(FName InValueName)
{
}

void UAIBlackboardBase::OnValueChanged(FName InValueName)
{
	OnRefresh();
	if(InValueName.IsEqual(FName("IsLostTarget")))
	{
		if(GetIsLostTarget() && GetTargetCharacter<AActor>())
		{
			SetTargetLocation(GetTargetCharacter<AActor>()->GetActorLocation());
		}
	}
	else if(InValueName.IsEqual(FName("TargetCharacter")))
	{
		if(!GetTargetCharacter())
		{
			SetIsLostTarget(false);
			SetTargetLocation(Vector_Empty);
		}
	}
}
