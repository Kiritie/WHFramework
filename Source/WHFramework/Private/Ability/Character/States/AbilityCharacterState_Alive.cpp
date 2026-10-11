#include "Ability/Character/States/AbilityCharacterState_Alive.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "AI/Base/AIControllerBase.h"
#include "Character/Components/CharacterMovementComponentBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "FSM/Components/FSMComponent.h"

UAbilityCharacterState_Alive::UAbilityCharacterState_Alive()
{
	StateName = TEXT("Alive");
}

bool UAbilityCharacterState_Alive::OnPreEnter(UFiniteStateBase* Last, const TArray<FParameter>& Params)
{
	const AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	return Super::OnPreEnter(Last, Params) && Character && Character->IsAbilityRuntimeReady();
}

void UAbilityCharacterState_Alive::OnEnter(UFiniteStateBase* Last, const TArray<FParameter>& Params)
{
	Super::OnEnter(Last, Params);
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	Character->OnLifecycleActivated();
	TryResumeMovement();
	if (AAIControllerBase* Controller = Cast<AAIControllerBase>(Character->GetController()))
	{
		Controller->RunBehaviorTree();
	}
}

void UAbilityCharacterState_Alive::OnLeave(UFiniteStateBase* Next)
{
	ClearMovementRetry();
	Super::OnLeave(Next);
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	if (AAIControllerBase* Controller = Cast<AAIControllerBase>(Character->GetController()))
	{
		Controller->StopBehaviorTree();
	}
}

void UAbilityCharacterState_Alive::OnPresentationEnter(UFiniteStateBase* Last, const FFSMReplicatedState& State)
{
    Super::OnPresentationEnter(Last, State);
    if (AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>())
    {
        Character->Execute_SetActorVisible(Character, true);
        TryResumeMovement();
    }
}

void UAbilityCharacterState_Alive::OnPresentationLeave(UFiniteStateBase* Next)
{
	ClearMovementRetry();
	Super::OnPresentationLeave(Next);
}

void UAbilityCharacterState_Alive::OnTermination()
{
	ClearMovementRetry();
	Super::OnTermination();
}

void UAbilityCharacterState_Alive::ClearMovementRetry()
{
	if (const AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>(); Character && Character->GetWorld())
	{
		Character->GetWorld()->GetTimerManager().ClearTimer(MovementRetryTimer);
	}
	MovementRetryTimer.Invalidate();
}

void UAbilityCharacterState_Alive::TryResumeMovement()
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	if (!Character || !Character->GetWorld() || !Character->GetFSMComponent() || Character->GetFSMComponent()->GetCurrentState() != this)
	{
		ClearMovementRetry();
		return;
	}
	UCharacterMovementComponentBase* Movement = CastChecked<UCharacterMovementComponentBase>(Character->GetCharacterMovement());
	const bool bSimulatedProxy = Character->GetLocalRole() == ROLE_SimulatedProxy;
	const bool bExternallyManaged = Character->IsMovementExternallyManaged();
	bool bReady = bSimulatedProxy || Character->IsAbilityRuntimeReady();
	if (bReady && !bExternallyManaged)
	{
		Character->SetActorEnableCollision(true);
		Character->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (!bSimulatedProxy && (!Movement->IsActive() || !Movement->IsComponentTickEnabled() || Movement->MovementMode == MOVE_None))
		{
			bReady = Movement->ResumeMovementFromLifecycle() == ECharacterWorldQueryResult::Ready;
		}
	}
	if (bReady)
	{
		Movement->Activate(true);
		Movement->SetComponentTickEnabled(true);
		ClearMovementRetry();
	}
	else
	{
		if (!bExternallyManaged)
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
			Movement->Deactivate();
		}
		if (!Character->GetWorld()->GetTimerManager().IsTimerActive(MovementRetryTimer))
		{
			Character->GetWorld()->GetTimerManager().SetTimer(MovementRetryTimer, this, &UAbilityCharacterState_Alive::TryResumeMovement, 0.05f, true);
		}
	}
	if (Character->IsLocallyControlled())
	{
		Character->Execute_SetBlockAllInput(Character, !bReady);
	}
}
