// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/AbilityVitalityBase.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/VitalityActionAbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
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
#include "Common/Interaction/InteractionComponent.h"

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

	Interaction->AddInteractAction(EInteractAction::Revive);

	// stats
	RaceID = NAME_None;
	GenerateVoxelID = FPrimaryAssetId();

	ActionAbilities = TMap<FGameplayTag, FVitalityActionAbilityData>();
}

void AAbilityVitalityBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	SwitchDefaultFiniteState();
}

void AAbilityVitalityBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	SwitchFiniteState(nullptr);

	RaceID = NAME_None;
	GenerateVoxelID = FPrimaryAssetId();
}

void AAbilityVitalityBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

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
	SwitchFiniteStateByClass<UAbilityVitalityState_Walk>();
}

void AAbilityVitalityBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void AAbilityVitalityBase::Death(IAbilityVitalityInterface* InKiller)
{
	SwitchFinalFiniteState({ InKiller });
}

void AAbilityVitalityBase::Kill(IAbilityVitalityInterface* InTarget)
{
	if(InTarget != this)
	{
		ModifyExp(InTarget->GetLevelA() * 10.f);
	}
	InTarget->Death(this);
}

void AAbilityVitalityBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	SwitchDefaultFiniteState({ InRescuer });
}

void AAbilityVitalityBase::Static()
{
	SwitchFiniteStateByClass<UAbilityVitalityState_Static>();
}

void AAbilityVitalityBase::UnStatic()
{
	if(IsCurrentFiniteStateClass<UAbilityVitalityState_Static>())
	{
		SwitchFiniteState(nullptr);
	}
}

void AAbilityVitalityBase::Interrupt(float InDuration /*= -1*/)
{
	SwitchFiniteStateByClass<UAbilityVitalityState_Interrupt>({ InDuration });
}

void AAbilityVitalityBase::UnInterrupt()
{
	if(IsCurrentFiniteStateClass<UAbilityVitalityState_Interrupt>())
	{
		SwitchFiniteState(nullptr);
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
		if(IsCurrentFiniteStateClass<UAbilityVitalityState_Death>())
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
	switch (InInteractAction)
	{
		case EInteractAction::Revive:
		{
			return IsDead();
		}
		default: break;
	}
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

	if(bPassive)
	{
		switch (InInteractAction)
		{
			case EInteractAction::Revive:
			{
				Revive(Cast<IAbilityVitalityInterface>(InInteractionAgent));
				break;
			}
			default: break;
		}
	}
}

void AAbilityVitalityBase::OnPreChangeItem(const FAbilityItem& InOldItem)
{
	Super::OnPreChangeItem(InOldItem);
}

void AAbilityVitalityBase::OnChangeItem(const FAbilityItem& InNewItem)
{
	Super::OnChangeItem(InNewItem);
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

void AAbilityVitalityBase::OnSelectItem(const FAbilityItem& InItem)
{
	Super::OnSelectItem(InItem);

	if(InItem.InventorySlot->GetSplitType() == ESlotSplitType::Shortcut)
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

bool AAbilityVitalityBase::OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	switch(InInteractEvent)
	{
		case EInputInteractEvent::Started:
		{
			return IVoxelAgentInterface::OnGenerateVoxel(InInteractEvent, InHitResult);
		}
		case EInputInteractEvent::Triggered:
		{
			return IVoxelAgentInterface::OnGenerateVoxel(InInteractEvent, InHitResult);
		}
		case EInputInteractEvent::Completed:
		{
			FItemQueryData ItemQueryData;
			if(GenerateVoxelItem.IsValid())
			{
				ItemQueryData = Inventory->QueryItemByRange(EItemQueryType::Remove, FAbilityItem(GenerateVoxelItem, 1), -1);
				if(!ItemQueryData.IsValid() || !bCanGenerateVoxel)
				{
					GenerateVoxelItem = FVoxelItem::Empty;
				}
			}
			if(IVoxelAgentInterface::OnGenerateVoxel(InInteractEvent, InHitResult))
			{
				Inventory->RemoveItemByQueryData(ItemQueryData);
				return true;
			}
			break;
		}
	}
	return false;
}

bool AAbilityVitalityBase::OnDestroyVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return IVoxelAgentInterface::OnDestroyVoxel(InInteractEvent, InHitResult);
}

void AAbilityVitalityBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	Super::OnAttributeChange(InAttributeChangeData);
}

void AAbilityVitalityBase::OnActorAttached(AActor* InActor)
{
	Super::OnActorAttached(InActor);
}

void AAbilityVitalityBase::OnActorDetached(AActor* InActor)
{
	Super::OnActorDetached(InActor);
}

void AAbilityVitalityBase::HandleDamage(const FGameplayAttribute& DamageAttribute, float DamageValue, float DefendValue, bool bHasCrited, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
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
		USceneModuleStatics::SpawnWorldText(FString::FromInt(DamageValue), UAbilityModuleStatics::GetAttributeColor(DamageAttribute), !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));
	}
	if(DefendValue >= 1.f)
	{
		USceneModuleStatics::SpawnWorldText(FString::FromInt(DefendValue), FColor::Cyan, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
	}
}

void AAbilityVitalityBase::HandleRecovery(const FGameplayAttribute& RecoveryAttribute, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if(RecoveryAttribute == GetHealthRecoveryAttribute())
	{
		ModifyHealth(RecoveryValue);
	
		if(RecoveryValue >= 1.f)
		{
			USceneModuleStatics::SpawnWorldText(FString::FromInt(RecoveryValue), FColor::Green, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
		}
	}
}

void AAbilityVitalityBase::HandleInterrupt(const FGameplayAttribute& InterruptAttribute, float InterruptDuration, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	
}

bool AAbilityVitalityBase::IsActive() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Active);
}

bool AAbilityVitalityBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dead) || (bCheckDying && IsDying());
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
