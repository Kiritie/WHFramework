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

void UAIBlackboardBase::Initialize(UBlackboardComponent* InComponent, ACharacterBase* InCharacter)
{
	Component = InComponent;
	Character = InCharacter;
	OnBlackboardValuePreChange.AddUObject(this, &UAIBlackboardBase::OnValuePreChange);
	OnBlackboardValueChanged.AddUObject(this, &UAIBlackboardBase::OnValueChanged);
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
}

void UAIBlackboardBase::OnValueChanged(FName InValueName)
{
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
