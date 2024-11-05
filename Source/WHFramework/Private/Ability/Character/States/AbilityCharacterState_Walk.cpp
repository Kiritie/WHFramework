// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Walk.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Effects/EffectBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

UAbilityCharacterState_Walk::UAbilityCharacterState_Walk()
{
	StateName = FName("Walk");
}

void UAbilityCharacterState_Walk::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Walk::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityCharacterState_Walk::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Walking);

	Character->DoAction(GameplayTags::Ability_Vitality_Action_Walk);

	if(Character->GetCharacterMovement()->MovementMode != MOVE_Walking)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	if(InLastState && InLastState->IsA<UAbilityCharacterState_Fall>())
	{
		UEffectBase* Effect = UObjectPoolModuleStatics::SpawnObject<UEffectBase>();

		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = GET_GAMEPLAYATTRIBUTE_PROPERTY(UVitalityAttributeSetBase, FallDamage);
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(1.f);

		Effect->Modifiers.Add(ModifierInfo);
		
		FGameplayEffectContextHandle EffectContext = Character->GetAbilitySystemComponent()->MakeEffectContext();
		EffectContext.AddSourceObject(Character);
		Character->GetAbilitySystemComponent()->ApplyGameplayEffectToSelf(Effect, 0, EffectContext);

		UObjectPoolModuleStatics::DespawnObject(Effect);
	}
}

void UAbilityCharacterState_Walk::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityCharacterState_Walk::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Walking);

	Character->StopAction(GameplayTags::Ability_Vitality_Action_Walk);
}

void UAbilityCharacterState_Walk::OnTermination()
{
	Super::OnTermination();
}
