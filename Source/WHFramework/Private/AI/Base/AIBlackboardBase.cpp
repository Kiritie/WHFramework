// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIBlackboardBase.h"

class UBlackboardKeyType_Class;

void UAIBlackboardBase::PostLoad()
{
	Super::PostLoad();
}

void UAIBlackboardBase::Initialize(UBlackboardComponent* InComponent, ACharacterBase* InOwnerCharacter)
{
	Component = InComponent;
	Character = InOwnerCharacter;

	Refresh();
}

void UAIBlackboardBase::Refresh()
{
	
}
