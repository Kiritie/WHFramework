// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/AbilityVitalityBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Death.h"
#include "Ability/Vitality/States/AbilityVitalityState_Spawn.h"
#include "Asset/AssetModuleStatics.h"
#include "FSM/Components/FSMComponent.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Datas/VoxelData.h"
#include "Ability/Vitality/AbilityVitalityInventoryBase.h"

AAbilityVitalityBase::AAbilityVitalityBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer.SetDefaultSubobjectClass<UVitalityAttributeSetBase>("AttributeSet").
		SetDefaultSubobjectClass<UAbilityVitalityInventoryBase>("Inventory"))
{
	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = FName("Vitality");
	
	FSM->DefaultState = UAbilityVitalityState_Spawn::StaticClass();
	FSM->FinalState = UAbilityVitalityState_Death::StaticClass();

	FSM->States.Add(UAbilityVitalityState_Spawn::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Death::StaticClass());

	// stats
	Name = NAME_None;
	RaceID = NAME_None;
	GenerateVoxelID = FPrimaryAssetId();
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
	Level = 0;

	Inventory->UnloadSaveData();
}

void AAbilityVitalityBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
	
	auto& SaveData = InSaveData->CastRef<FVitalitySaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		SetNameV(SaveData.Name);
		SetRaceID(SaveData.RaceID);
	}
}

FSaveData* AAbilityVitalityBase::ToData()
{
	static FVitalitySaveData SaveData;
	SaveData = Super::ToData()->CastRef<FActorSaveData>();

	SaveData.Name = Name;
	SaveData.RaceID = RaceID;

	return &SaveData;
}

void AAbilityVitalityBase::ResetData()
{
	SetHealth(GetMaxHealth());
}

void AAbilityVitalityBase::OnFiniteStateRefresh(UFiniteStateBase* InCurrentState)
{
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
	
	if(InItem.IsValid() && InItem.GetType() == EAbilityItemType::Voxel)
	{
		SetGenerateVoxelID(InItem.ID);
	}
	else
	{
		SetGenerateVoxelID(FPrimaryAssetId());
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

bool AAbilityVitalityBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::StateTag_Vitality_Dead) || bCheckDying && IsDying();
}

bool AAbilityVitalityBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::StateTag_Vitality_Dying);
}

bool AAbilityVitalityBase::SetLevelA(int32 InLevel)
{
	const auto& VitalityData = GetVitalityData<UAbilityVitalityDataBase>();
	InLevel = FMath::Clamp(InLevel, 0, VitalityData.MaxLevel != -1 ? VitalityData.MaxLevel : InLevel);

	if(Level != InLevel)
	{
		Level = InLevel;

		auto EffectContext = AbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		auto SpecHandle = AbilitySystem->MakeOutgoingSpec(VitalityData.PEClass, InLevel, EffectContext);
		if (SpecHandle.IsValid())
		{
			AbilitySystem->BP_ApplyGameplayEffectSpecToSelf(SpecHandle);
		}

		return true;
	}
	return false;
}

FString AAbilityVitalityBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

UAbilityVitalityDataBase& AAbilityVitalityBase::GetVitalityData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityVitalityDataBase>(AssetID);
}

void AAbilityVitalityBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if(InAttributeChangeData.Attribute == GetExpAttribute())
	{
		if(InAttributeChangeData.NewValue >= GetMaxExp())
		{
			SetLevelA(GetLevelA() + 1);
			SetExp(0.f);
		}
	}
}

void AAbilityVitalityBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-LocalDamageDone);

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

	USceneModuleStatics::SpawnWorldText(FString::FromInt(LocalDamageDone), FColor::White, !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));
}

void AAbilityVitalityBase::HandleRecovery(const float LocalRecoveryDone, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(LocalRecoveryDone);
	
	USceneModuleStatics::SpawnWorldText(FString::FromInt(LocalRecoveryDone), FColor::Green, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
}

void AAbilityVitalityBase::HandleInterrupt(const float InterruptDuration, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
}
