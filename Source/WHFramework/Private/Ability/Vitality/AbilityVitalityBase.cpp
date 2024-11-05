// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/AbilityVitalityBase.h"

#include "Ability/Abilities/VitalityActionAbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Death.h"
#include "Ability/Vitality/States/AbilityVitalityState_Spawn.h"
#include "Asset/AssetModuleStatics.h"
#include "FSM/Components/FSMComponent.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Ability/Vitality/AbilityVitalityInventoryBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Interrupt.h"
#include "Ability/Vitality/States/AbilityVitalityState_Static.h"
#include "Ability/Vitality/States/AbilityVitalityState_Walk.h"

AAbilityVitalityBase::AAbilityVitalityBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer.SetDefaultSubobjectClass<UVitalityAttributeSetBase>(FName("AttributeSet")).
		SetDefaultSubobjectClass<UAbilityVitalityInventoryBase>(FName("Inventory")))
{
	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = TEXT("Vitality");
	
	FSM->DefaultState = UAbilityVitalityState_Spawn::StaticClass();
	FSM->FinalState = UAbilityVitalityState_Death::StaticClass();

	FSM->States.Add(UAbilityVitalityState_Death::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Interrupt::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Spawn::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Static::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Walk::StaticClass());

	// stats
	RaceID = NAME_None;
	GenerateVoxelID = FPrimaryAssetId();

	ActionAbilities = TMap<FGameplayTag, FVitalityActionAbilityData>();
}

void AAbilityVitalityBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	FSM->SwitchDefaultState();
}

void AAbilityVitalityBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	FSM->SwitchState(nullptr);

	RaceID = NAME_None;
	GenerateVoxelID = FPrimaryAssetId();
}

void AAbilityVitalityBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FVitalitySaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		if(SaveData.IsSaved())
		{
			ActionAbilities = SaveData.ActionAbilities;
			for(auto& Iter : ActionAbilities)
			{
				Iter.Value.AbilityHandle = AbilitySystem->K2_GiveAbility(Iter.Value.AbilityClass, Iter.Value.Level);
			}
		}
		else
		{
			const UAbilityVitalityDataBase& VitalityData = GetVitalityData<UAbilityVitalityDataBase>();
			
			for(auto Iter : VitalityData.ActionAbilities)
			{
				Iter.AbilityHandle = AbilitySystem->K2_GiveAbility(Iter.AbilityClass, Iter.Level);
				ActionAbilities.Add(Iter.AbilityClass->GetDefaultObject<UAbilityBase>()->AbilityTags.GetByIndex(0), Iter);
			}
		}
	}
	if(PHASEC(InPhase, EPhase::All))
	{
		SetRaceID(SaveData.RaceID);
	}

	Super::LoadData(InSaveData, InPhase);
}

FSaveData* AAbilityVitalityBase::ToData()
{
	static FVitalitySaveData SaveData;
	SaveData = Super::ToData()->CastRef<FActorSaveData>();

	SaveData.RaceID = RaceID;

	return &SaveData;
}

void AAbilityVitalityBase::ResetData()
{
	SetHealth(GetMaxHealth());
}

void AAbilityVitalityBase::OnFiniteStateRefresh(UFiniteStateBase* InCurrentState)
{
	FSM->SwitchStateByClass<UAbilityVitalityState_Walk>();
}

void AAbilityVitalityBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void AAbilityVitalityBase::Death(IAbilityVitalityInterface* InKiller)
{
	FSM->SwitchFinalState({ InKiller });
}

void AAbilityVitalityBase::Kill(IAbilityVitalityInterface* InTarget)
{
	InTarget->Death(this);
}

void AAbilityVitalityBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	FSM->SwitchDefaultState();
}

void AAbilityVitalityBase::Static()
{
	FSM->SwitchStateByClass<UAbilityVitalityState_Static>();
}

void AAbilityVitalityBase::UnStatic()
{
	if(FSM->IsCurrentStateClass<UAbilityVitalityState_Static>())
	{
		FSM->SwitchState(nullptr);
	}
}

void AAbilityVitalityBase::Interrupt(float InDuration /*= -1*/)
{
	FSM->SwitchStateByClass<UAbilityVitalityState_Interrupt>({ InDuration });
}

void AAbilityVitalityBase::UnInterrupt()
{
	if(FSM->IsCurrentStateClass<UAbilityVitalityState_Interrupt>())
	{
		FSM->SwitchState(nullptr);
	}
}

bool AAbilityVitalityBase::DoAction(const FGameplayTag& InActionTag)
{
	if(!HasActionAbility(InActionTag)) return false;
	
	const FVitalityActionAbilityData AbilityData = GetActionAbility(InActionTag);
	const bool bSuccess = AbilitySystem->TryActivateAbility(AbilityData.AbilityHandle);
	const FGameplayAbilitySpec Spec = AbilitySystem->FindAbilitySpecForHandle(AbilityData.AbilityHandle);
	return bSuccess;
}

bool AAbilityVitalityBase::StopAction(const FGameplayTag& InActionTag)
{
	if(!HasActionAbility(InActionTag)) return false;
	
	const FVitalityActionAbilityData AbilityData = GetActionAbility(InActionTag);
	const FGameplayAbilitySpec Spec = AbilitySystem->FindAbilitySpecForHandle(AbilityData.AbilityHandle);
	if(UVitalityActionAbilityBase* Ability = Cast<UVitalityActionAbilityBase>(Spec.GetPrimaryInstance()))
	{
		Ability->SetStopped(true);
	}
	AbilitySystem->CancelAbilityHandle(AbilityData.AbilityHandle);
	return true;
}

void AAbilityVitalityBase::EndAction(const FGameplayTag& InActionTag, bool bWasCancelled)
{
	if(InActionTag.MatchesTag(GameplayTags::Ability_Vitality_Action_Death))
	{
		if(FSM->IsCurrentStateClass<UAbilityVitalityState_Death>())
		{
			FSM->GetCurrentState<UAbilityVitalityState_Death>()->DeathEnd();
		}
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Vitality_Action_Static))
	{
		UnStatic();
	}
	else if(InActionTag.MatchesTag(GameplayTags::Ability_Vitality_Action_Interrupt))
	{
		UnInterrupt();
	}
}

bool AAbilityVitalityBase::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	return Super::CanInteract(InInteractAction, InInteractionAgent);
}

void AAbilityVitalityBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
	Super::OnEnterInteract(InInteractionAgent);
}

void AAbilityVitalityBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
	Super::OnLeaveInteract(InInteractionAgent);
}

void AAbilityVitalityBase::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	Super::OnInteract(InInteractAction, InInteractionAgent, bPassive);
}

void AAbilityVitalityBase::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{
	Super::OnActiveItem(InItem, bPassive, bSuccess);
}

void AAbilityVitalityBase::OnRemoveItem(const FAbilityItem& InItem)
{
	Super::OnRemoveItem(InItem);
}

void AAbilityVitalityBase::OnDeactiveItem(const FAbilityItem& InItem, bool bPassive)
{
	Super::OnDeactiveItem(InItem, bPassive);
}

void AAbilityVitalityBase::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	Super::OnDiscardItem(InItem, bInPlace);
}

void AAbilityVitalityBase::OnSelectItem(ESlotSplitType InSplitType, const FAbilityItem& InItem)
{
	Super::OnSelectItem(InSplitType, InItem);

	if(InSplitType == ESlotSplitType::Shortcut)
	{
		if(InItem.IsValid() && InItem.GetType() == EAbilityItemType::Voxel)
		{
			SetGenerateVoxelID(InItem.ID);
		}
		else
		{
			SetGenerateVoxelID(FPrimaryAssetId());
		}
	}
}

void AAbilityVitalityBase::OnAuxiliaryItem(const FAbilityItem& InItem)
{
	Super::OnAuxiliaryItem(InItem);
}

bool AAbilityVitalityBase::OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnGenerateVoxel(InVoxelHitResult);
}

bool AAbilityVitalityBase::OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnDestroyVoxel(InVoxelHitResult);
}

void AAbilityVitalityBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	Super::OnAttributeChange(InAttributeChangeData);
}

void AAbilityVitalityBase::HandleDamage(EDamageType DamageType, float DamageValue, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-DamageValue);

	if (GetHealth() <= 0.f)
	{
		if(IAbilityVitalityInterface* SourceVitality = Cast<IAbilityVitalityInterface>(SourceActor))
		{
			SourceVitality->Kill(this);
		}
		else
		{
			Death(nullptr);
		}
	}

	if(DamageValue >= 1.f)
	{
		USceneModuleStatics::SpawnWorldText(FString::FromInt(DamageValue), DamageType != EDamageType::Magic ? FColor::Red : FColor::Cyan, !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));
	}
}

void AAbilityVitalityBase::HandleRecovery(float RecoveryValue, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(RecoveryValue);
	
	if(RecoveryValue >= 1.f)
	{
		USceneModuleStatics::SpawnWorldText(FString::FromInt(RecoveryValue), FColor::Green, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
	}
}

void AAbilityVitalityBase::HandleInterrupt(float InterruptDuration, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
}

bool AAbilityVitalityBase::IsActive(bool bNeedNotDead) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Active) && (!bNeedNotDead || !IsDead());
}

bool AAbilityVitalityBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dead) || bCheckDying && IsDying();
}

bool AAbilityVitalityBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dying);
}

bool AAbilityVitalityBase::IsWalking(bool bReally) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Walking);
}

bool AAbilityVitalityBase::IsInterrupting() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

FString AAbilityVitalityBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

bool AAbilityVitalityBase::HasActionAbility(const FGameplayTag& InActionTag) const
{
	return ActionAbilities.Contains(InActionTag);
}

FVitalityActionAbilityData AAbilityVitalityBase::GetActionAbility(const FGameplayTag& InActionTag)
{
	if(HasActionAbility(InActionTag))
	{
		return ActionAbilities[InActionTag];
	}
	return FVitalityActionAbilityData();
}

UAbilityVitalityDataBase& AAbilityVitalityBase::GetVitalityData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityVitalityDataBase>(AssetID);
}
