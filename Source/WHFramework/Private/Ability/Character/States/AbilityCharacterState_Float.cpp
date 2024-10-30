// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Float.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ability/Character/States/AbilityCharacterState_Swim.h"
#include "FSM/Components/FSMComponent.h"

UAbilityCharacterState_Float::UAbilityCharacterState_Float()
{
	StateName = FName("Float");

	WaterPosZ = -1.f;
}

void UAbilityCharacterState_Float::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Float::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->DoAction(GameplayTags::Ability_Character_Action_Float);
}

void UAbilityCharacterState_Float::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if(InParams.IsValidIndex(0))
	{
		WaterPosZ = InParams[0];
	}

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Character_Floating);

	if(Character->GetCharacterMovement()->MovementMode != MOVE_Swimming)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);
	}

	Character->GetCharacterMovement()->Velocity.Z = 0;
}

void UAbilityCharacterState_Float::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	if(WaterPosZ != -1.f)
	{
		const float OffsetPosZ = GetFloatOffsetPosZ();
		const float TargetPosZ = WaterPosZ - OffsetPosZ;
		Character->SetActorLocation(FVector(Character->GetActorLocation().X, Character->GetActorLocation().Y, FMath::FInterpConstantTo(Character->GetActorLocation().Z, TargetPosZ, DeltaSeconds, Character->GetSwimSpeed())));
		if(FMath::IsNearlyEqual(Character->GetActorLocation().Z, TargetPosZ, 0.2f))
		{
			WaterPosZ = -1.f;
		}
	}
}

void UAbilityCharacterState_Float::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->StopAction(GameplayTags::Ability_Character_Action_Float);
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Character_Floating);
}

void UAbilityCharacterState_Float::OnTermination()
{
	Super::OnTermination();
}

float UAbilityCharacterState_Float::GetFloatOffsetPosZ() const
{
	return 0.f;
}
