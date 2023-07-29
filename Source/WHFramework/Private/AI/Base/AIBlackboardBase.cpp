// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIBlackboardBase.h"

#include "Math/MathTypes.h"
#include "Character/Base/CharacterBase.h"

class UBlackboardKeyType_Class;

void UAIBlackboardBase::PostLoad()
{
	Super::PostLoad();

	// BLACKBOARD_VALUE_GENERATE_BOOL(IsLostTarget);
	// BLACKBOARD_VALUE_GENERATE_VECTOR(TargetLocation);
	// BLACKBOARD_VALUE_GENERATE_OBJECT(TargetCharacter, ACharacterBase);
}

void UAIBlackboardBase::Initialize(UBlackboardComponent* InComponent, ACharacterBase* InCharacter)
{
	Component = InComponent;
	Character = InCharacter;
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
			if(GetTargetCharacter<ACharacterBase>())
			{
				SetTargetLocation(GetTargetCharacter<ACharacterBase>()->GetActorLocation());
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
