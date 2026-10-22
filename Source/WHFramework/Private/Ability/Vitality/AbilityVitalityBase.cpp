// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Components/AbilityControlComponent.h"
#include "Net/UnrealNetwork.h"
#include "Ability/Effects/EffectBase.h"
#include "Voxel/Agent/VoxelAgentComponent.h"

#include "Ability/Interaction/AbilityInteractionOptions.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/VitalityActionAbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Death.h"
#include "Ability/Vitality/States/AbilityVitalityState_Spawn.h"
#include "Asset/AssetModuleStatics.h"
#include "FSM/Components/FSMComponent.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Ability/Vitality/States/AbilityVitalityState_Walk.h"
#include "Common/Interaction/InteractionComponent.h"

AAbilityVitalityBase::AAbilityVitalityBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UVitalityAttributeSetBase>(FName("AttributeSet")))
{
	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = TEXT("Vitality");
	FSM->DomainName = TEXT("Lifecycle");
	FSM->bAutoSwitchDefault = false;
	Control = CreateDefaultSubobject<UAbilityControlComponent>(TEXT("Control"));

	FSM->DefaultState = UAbilityVitalityState_Spawn::StaticClass();
	FSM->FinalState = UAbilityVitalityState_Death::StaticClass();

	FSM->States.Add(UAbilityVitalityState_Death::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Spawn::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Walk::StaticClass());

	Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_AbilityRevive>(TEXT("ReviveOption")));

	// stats
	RaceID = NAME_None;
	VoxelAgentComponent = CreateDefaultSubobject<UVoxelAgentComponent>(TEXT("VoxelAgentComponent"));

}

void AAbilityVitalityBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
	if (VoxelAgentComponent)
	{
		VoxelAgentComponent->SetAgentEnabled(true);
	}

	AbilitySystem->SetAbilityEntityId(ActorID);
	if (HasAuthority())
	{
		SwitchDefaultFiniteState();
	}
}

UVoxelAgentComponent* AAbilityVitalityBase::GetVoxelAgentComponent() const
{
	return VoxelAgentComponent;
}

void AAbilityVitalityBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Control->ResetControls();

	if (HasAuthority())
	{
		AbilitySystem->RemoveGrantSource(GetDefaultGrantSourceId());
		AbilitySystem->RemoveGrantSource(GetProgressionGrantSourceId());
	}

	Super::OnDespawn_Implementation(InMode);

	SwitchFiniteState(nullptr);

	RaceID = NAME_None;
	if (VoxelAgentComponent)
	{
		VoxelAgentComponent->SetAgentEnabled(false);
	}
}

void AAbilityVitalityBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityVitalityBase::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AAbilityVitalityBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if (HasAuthority() && IsActive() && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.f)
	{
		ModifyHealth(ATTRIBUTE_DELTAVALUE_CLAMP(this, Health, GetHealthRegenSpeed() * DeltaSeconds));
	}
}

void AAbilityVitalityBase::OnTermination_Implementation()
{
	Control->ResetControls();

	if (HasAuthority())
	{
		AbilitySystem->RemoveGrantSource(GetDefaultGrantSourceId());
		AbilitySystem->RemoveGrantSource(GetProgressionGrantSourceId());
	}

	Super::OnTermination_Implementation();
}

void AAbilityVitalityBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if (!HasAuthority())
	{
		return;
	}
	Super::LoadData(InSaveData, InPhase);
	const FVitalitySaveData* SaveData = InSaveData.GetPtr<FVitalitySaveData>();
	if (!SaveData)
	{
		return;
	}
	if (PHASEC(InPhase, EPhase::Primary))
	{
		AbilitySystem->SetAbilityEntityId(ActorID);
		const UAbilityVitalityDataBase& Data = GetVitalityData();
		TArray<FAbilityGrantDefinition> Grants;
		for (const FVitalityActionAbilityData& Action : Data.ActionAbilities)
		{
			FAbilityGrantDefinition Grant;
			Grant.DefinitionId = Action.ID;
			Grant.AbilityClass = Action.AbilityClass;
			Grant.SourceObject = const_cast<UAbilityVitalityDataBase*>(&Data);
			Grant.Level = Action.Level;
			Grants.Add(Grant);
		}
		if (!AbilitySystem->SetGrantSource(GetDefaultGrantSourceId(), Grants, {}, 1))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid action definitions on %s"), *GetName());
			return;
		}
	}
	if (PHASEC(InPhase, EPhase::All))
	{
		SetRaceID(SaveData->RaceID);
	}
}

FParameter AAbilityVitalityBase::ToData()
{
	FVitalitySaveData SaveData;
	const FParameter ParentData = Super::ToData();
	const FActorSaveData* Parent = ParentData.GetPtr<FActorSaveData>();
	if (!Parent)
	{
		return FParameter();
	}
	SaveData = *Parent;

	SaveData.RaceID = RaceID;

	return FParameter(MoveTemp(SaveData));
}

void AAbilityVitalityBase::ResetData()
{
	SetHealth(GetMaxHealth());
}

void AAbilityVitalityBase::OnFiniteStateRefresh(UFiniteStateBase* InCurrentState)
{
	if (!IsDead())
	{
		SwitchFiniteStateByClass<UAbilityVitalityState_Walk>();
	}
}

void AAbilityVitalityBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void AAbilityVitalityBase::Death(IAbilityVitalityInterface* InKiller)
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	const FGuid DyingIdentity = ActorID;
	UAbilitySystemComponentBase* const DyingSystem = AbilitySystem;
	const auto IsSameDeath = [this, DyingIdentity, DyingSystem]()
	{
		return IsValid(this) && ActorID == DyingIdentity && AbilitySystem == DyingSystem &&
			DyingSystem->GetAvatarActor() == this && IsDead();
	};
	SwitchFinalFiniteState({Cast<UObject>(InKiller)});
	if (!IsSameDeath())
	{
		return;
	}
	Control->ResetControls();
	if (!IsSameDeath())
	{
		return;
	}
	const TArray<FAbilityGrantedEntry> Entries = DyingSystem->GetGrantedEntries();
	for (const FAbilityGrantedEntry& Entry : Entries)
	{
		const FGameplayAbilitySpec* Spec = DyingSystem->FindAbilitySpecFromHandle(Entry.AbilityHandle);
		if (UAbilityBase* Action = Spec ? Cast<UAbilityBase>(Spec->GetPrimaryInstance()) : nullptr; Action && Action->IsActive())
		{
			Action->FailAction(AbilityActionTags::Blocked);
			if (!IsSameDeath())
			{
				return;
			}
		}
	}
	if (IsSameDeath())
	{
		DyingSystem->RemoveEffectsOnDeath();
	}
}

void AAbilityVitalityBase::Kill(IAbilityVitalityInterface* InTarget)
{
	if (!HasAuthority() || !InTarget || InTarget->IsDead())
	{
		return;
	}

	if (InTarget != this)
	{
		ModifyExp(InTarget->GetLevelA() * 10.f);
	}
	InTarget->Death(this);
}

void AAbilityVitalityBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	if (!HasAuthority() || !IsDead())
	{
		return;
	}

	SwitchDefaultFiniteState({Cast<UObject>(InRescuer)});
}

void AAbilityVitalityBase::Static()
{
	Control->SetStatic(true);
}

void AAbilityVitalityBase::UnStatic()
{
	Control->SetStatic(false);
}

void AAbilityVitalityBase::Interrupt(float InDuration)
{
	if (FMath::IsFinite(InDuration))
	{
		Control->AddInterrupt(InDuration > 0.f ? InDuration : 0.35f);
	}
}

void AAbilityVitalityBase::UnInterrupt()
{
	Control->ClearInterrupts();
}

bool AAbilityVitalityBase::DoAction(const FGameplayTag& InActionTag)
{
	FGameplayAbilitySpecHandle Handle;
	if (!AbilitySystem->FindGrantedAbility(FPrimaryAssetId(), InActionTag, Handle))
	{
		return false;
	}
	const FAbilityGrantedEntry* Entry = AbilitySystem->GetGrantedEntries().FindByPredicate([Handle](const FAbilityGrantedEntry& Value) { return Value.AbilityHandle == Handle; });
	if (!Entry)
	{
		return false;
	}
	FAbilityActionRequest Request;
	Request.AbilityHandle = Handle;
	Request.DefinitionId = Entry->DefinitionId;
	Request.ActionTag = Entry->ActionTag;
	Request.TargetActor = this;
	Request.TargetLocation = GetActorLocation();
	Request.AimDirection = GetActorForwardVector();
	return AbilitySystem->RequestAction(Request).bAccepted;
}

bool AAbilityVitalityBase::StopAction(const FGameplayTag& InActionTag)
{
	bool bCancelled = false;
	const TArray<FAbilityGrantedEntry> Entries = AbilitySystem->GetGrantedEntries();
	for (const FAbilityGrantedEntry& Entry : Entries)
	{
		if (Entry.ActionTag != InActionTag)
		{
			continue;
		}
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Entry.AbilityHandle);
		UAbilityBase* Action = Spec ? Cast<UAbilityBase>(Spec->GetPrimaryInstance()) : nullptr;
		if (Action && Action->IsActive())
		{
			const FGuid Id = Action->GetActionActivationId();
			bCancelled |= AbilitySystem->CancelAction(Id, AbilityActionTags::Cancelled);
		}
	}
	return bCancelled;
}



void AAbilityVitalityBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
	Super::OnEnterInteract(InInteractionAgent);
}

void AAbilityVitalityBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
	Super::OnLeaveInteract(InInteractionAgent);
}


void AAbilityVitalityBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if (!HasAuthority())
	{
		return;
	}

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

void AAbilityVitalityBase::HandleDamage(const FGameplayAttribute& DamageAttribute,
                                        float DamageValue,
                                        float DefendValue,
                                        bool bHasCrited,
                                        const FHitResult& HitResult,
                                        const FGameplayTagContainer& SourceTags,
                                        AActor* SourceActor)
{
	if (!HasAuthority() || IsDead() || !FMath::IsFinite(DamageValue) || DamageValue <= 0.f)
	{
		return;
	}

	ModifyHealth(-DamageValue);

	if (GetHealth() <= 0.f)
	{
		if (IAbilityVitalityInterface* SourceVitality = Cast<IAbilityVitalityInterface>(SourceActor))
		{
			SourceVitality->Kill(this);
		}
		else
		{
			Death(nullptr);
		}
	}

	if (DamageValue >= 1.f)
	{
		USceneModuleStatics::SpawnWorldText(FString::FromInt(DamageValue),
		                                    UAbilityModuleStatics::GetAttributeColor(DamageAttribute),
		                                    !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress,
		                                    GetActorLocation(),
		                                    FVector(20.f));
	}
	if (DefendValue >= 1.f)
	{
		USceneModuleStatics::SpawnWorldText(FString::FromInt(DefendValue), FColor::Cyan, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
	}
}

void AAbilityVitalityBase::HandleRecovery(
    const FGameplayAttribute& RecoveryAttribute, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if (!HasAuthority() || IsDead() || !FMath::IsFinite(RecoveryValue) || RecoveryValue <= 0.f)
	{
		return;
	}

	if (RecoveryAttribute == GetHealthRecoveryAttribute())
	{
		ModifyHealth(RecoveryValue);

		if (RecoveryValue >= 1.f)
		{
			USceneModuleStatics::SpawnWorldText(FString::FromInt(RecoveryValue), FColor::Green, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
		}
	}
}

void AAbilityVitalityBase::HandleInterrupt(const FGameplayAttribute& InterruptAttribute,
                                           float InterruptDuration,
                                           const FHitResult& HitResult,
                                           const FGameplayTagContainer& SourceTags,
                                           AActor* SourceActor)
{
	if (HasAuthority() && !IsDead() && FMath::IsFinite(InterruptDuration) && InterruptDuration > 0.f)
	{
		Interrupt(InterruptDuration);
	}
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
	return false;
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
	FGameplayAbilitySpecHandle Handle;
	return AbilitySystem->FindGrantedAbility(FPrimaryAssetId(), InActionTag, Handle);
}

FVitalityActionAbilityData AAbilityVitalityBase::GetActionAbility(const FGameplayTag& InActionTag)
{
	const TMap<FGameplayTag, FVitalityActionAbilityData> Projection = GetActionAbilities();
	const FVitalityActionAbilityData* Data = Projection.Find(InActionTag);
	return Data ? *Data : FVitalityActionAbilityData();
}

UAbilityVitalityDataBase& AAbilityVitalityBase::GetVitalityData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityVitalityDataBase>(AssetID);
}


TMap<FGameplayTag, FVitalityActionAbilityData> AAbilityVitalityBase::GetActionAbilities() const
{
	TMap<FGameplayTag, FVitalityActionAbilityData> Result;
	for (const FAbilityGrantedEntry& Entry : AbilitySystem->GetGrantedEntries())
	{
		FGameplayAbilitySpecHandle Selected;
		if (!AbilitySystem->FindGrantedAbility(FPrimaryAssetId(), Entry.ActionTag, Selected) || Selected != Entry.AbilityHandle)
		{
			continue;
		}
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Selected);
		if (Spec && Spec->Ability)
		{
			FVitalityActionAbilityData Data;
			Data.ID = Entry.DefinitionId;
			Data.Level = Entry.Level;
			Data.AbilityClass = Spec->Ability->GetClass();
			Data.AbilityHandle = Selected;
			Result.Add(Entry.ActionTag, Data);
		}
	}
	return Result;
}

FGuid AAbilityVitalityBase::GetDefaultGrantSourceId() const
{
	return FGuid(ActorID.A ^ 0x1D305B4Eu, ActorID.B, ActorID.C, ActorID.D ^ 0x083A49C2u);
}

FGuid AAbilityVitalityBase::GetProgressionGrantSourceId() const
{
	return FGuid(ActorID.A ^ 0x6C02753Du, ActorID.B, ActorID.C, ActorID.D ^ 0x732749E1u);
}

void AAbilityVitalityBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityVitalityBase, RaceID);
}

bool AAbilityVitalityBase::SetLevelA(int32 InLevel)
{
	if (!HasAuthority())
	{
		return false;
	}
	const auto& Data = GetVitalityData();
	InLevel = Data.ClampLevel(InLevel);
	if (InLevel == Level)
	{
		return false;
	}
	TArray<TSubclassOf<UEffectBase>> Effects;
	if (Data.PEClass)
	{
		Effects.Add(Data.PEClass);
	}
	if (!AbilitySystem->SetGrantSource(GetProgressionGrantSourceId(), {}, Effects, InLevel))
	{
		return false;
	}
	Level = InLevel;
	SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	ForceNetUpdate();
	return true;
}
