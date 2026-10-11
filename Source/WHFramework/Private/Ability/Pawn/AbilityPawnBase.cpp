// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/AbilityPawnBase.h"
#include "Net/UnrealNetwork.h"
#include "Ability/Effects/EffectBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Engine/World.h"

#include "Ability/Interaction/AbilityInteractionOptions.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Ability/Pawn/AbilityPawnDataBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Death.h"
#include "Ability/Pawn/States/AbilityPawnState_Spawn.h"
#include "Components/BoxComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Common/CommonModuleStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/VitalityActionAbilityBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Interrupt.h"
#include "Ability/Pawn/States/AbilityPawnState_Static.h"
#include "Ability/Pawn/States/AbilityPawnState_Walk.h"

AAbilityPawnBase::AAbilityPawnBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	AutoPossessAI = EAutoPossessAI::Disabled;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UVitalityAttributeSetBase>(FName("AttributeSet"));

	Inventory = nullptr;

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetInteractable(false);

	Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_AbilityRevive>(TEXT("ReviveOption")));

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = FName("Vitality");
	FSM->DomainName = TEXT("Lifecycle");
	FSM->bAutoSwitchDefault = false;
	ControlFSM = CreateDefaultSubobject<UFSMComponent>(TEXT("ControlFSM"));
	ControlFSM->DomainName = TEXT("Control");
	ControlFSM->bAutoSwitchDefault = false;
	ControlFSM->States.Add(UAbilityPawnState_Static::StaticClass());
	ControlFSM->States.Add(UAbilityPawnState_Interrupt::StaticClass());

	FSM->DefaultState = UAbilityPawnState_Spawn::StaticClass();
	FSM->FinalState = UAbilityPawnState_Death::StaticClass();

	FSM->States.Add(UAbilityPawnState_Death::StaticClass());
	FSM->States.Add(UAbilityPawnState_Spawn::StaticClass());
	FSM->States.Add(UAbilityPawnState_Walk::StaticClass());

	// stats
	RaceID = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;

	MovementRate = 1.f;
	RotationRate = 1.f;

}

void AAbilityPawnBase::OnSpawn_Implementation(const FParameter& InParam)
{
	if (HasAuthority() && AttributeSet)
	{
		const UAttributeSetBase* Defaults = AttributeSet->GetClass()->GetDefaultObject<UAttributeSetBase>();
		TArray<FGameplayAttribute> Attributes = AttributeSet->GetAllAttributes();
		Attributes.StableSort([](const FGameplayAttribute& A, const FGameplayAttribute& B) { return A.GetName().StartsWith(TEXT("Max")) && !B.GetName().StartsWith(TEXT("Max")); });
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			if (const FGameplayAttributeData* Value = Attribute.GetGameplayAttributeData(Defaults))
			{
				AbilitySystem->SetNumericAttributeBase(Attribute, Value->GetBaseValue());
			}
		}
	}

	ControlFSM->OnInitialize();

	Super::OnSpawn_Implementation(InParam);

	AbilitySystem->SetAbilityEntityId(ActorID);
	InitializeAbilities();
	if (HasAuthority())
	{
		SpawnDefaultController();
		SwitchDefaultFiniteState();
	}
}

void AAbilityPawnBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	ControlFSM->OnTermination();

	UninitializeAbilities();
	if (HasAuthority())
	{
		AbilitySystem->RemoveGrantSource(GetDefaultGrantSourceId());
		AbilitySystem->RemoveGrantSource(GetProgressionGrantSourceId());
		AbilitySystem->ClearAllAbilities();
		for (FActiveGameplayEffectHandle Effect : AbilitySystem->GetActiveEffects(FGameplayEffectQuery()))
		{
			AbilitySystem->RemoveActiveGameplayEffect(Effect);
		}
	}

	BindInventory(nullptr);
	RaceID = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;

	Super::OnDespawn_Implementation(InMode);
}

void AAbilityPawnBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityPawnBase::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	RefreshAttributes();
}

void AAbilityPawnBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if (HasAuthority() && IsActive() && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.f)
	{
		ModifyHealth(ATTRIBUTE_DELTAVALUE_CLAMP(this, Health, GetHealthRegenSpeed() * DeltaSeconds));

		if (GetMoveVelocity(true).Size() > 0.2f)
		{
			if (!IsMoving())
			{
				AbilitySystem->SetLooseGameplayTagCount(GameplayTags::State_Pawn_Moving, 1, EGameplayTagReplicationState::TagAndCountToAll);
			}
		}
		else if (IsMoving())
		{
			AbilitySystem->SetLooseGameplayTagCount(GameplayTags::State_Pawn_Moving, 0, EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
}

void AAbilityPawnBase::OnTermination_Implementation()
{
	BindInventory(nullptr);
	ControlFSM->OnTermination();

	UninitializeAbilities();
	if (HasAuthority())
	{
		AbilitySystem->RemoveGrantSource(GetDefaultGrantSourceId());
		AbilitySystem->RemoveGrantSource(GetProgressionGrantSourceId());
	}

	Super::OnTermination_Implementation();
}

void AAbilityPawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BindASCInput();
}

void AAbilityPawnBase::BindASCInput()
{
	if (!bASCInputBound && IsValid(AbilitySystem) && IsValid(InputComponent))
	{
		AbilitySystem->BindAbilityActivationToInputComponent(InputComponent,
		                                                     FGameplayAbilityInputBinds(FString("ConfirmTarget"),
		                                                                                FString("CancelTarget"),
		                                                                                FTopLevelAssetPath("/Script/WHFramework", FName("EAbilityInputID")),
		                                                                                static_cast<int32>(EAbilityInputID::Confirm),
		                                                                                static_cast<int32>(EAbilityInputID::Cancel)));
		bASCInputBound = true;
	}
}

void AAbilityPawnBase::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void AAbilityPawnBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.ArIsSaveGame)
	{
		if (Ar.IsLoading())
		{
			Ar << Level;
		}
		else if (Ar.IsSaving())
		{
			Ar << Level;
		}
		AttributeSet->SerializeAttributes(Ar);
	}
}

void AAbilityPawnBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	const FPawnSaveData* SaveData = InSaveData.GetPtr<FPawnSaveData>();
	if (!SaveData || !HasAuthority())
	{
		return;
	}
	if (PHASEC(InPhase, EPhase::Primary))
	{
		SetActorTransform(SaveData->SpawnTransform);
		BirthTransform = SaveData->IsSaved() ? SaveData->BirthTransform : SaveData->SpawnTransform;
		AbilitySystem->SetAbilityEntityId(ActorID);
		const UAbilityPawnDataBase& Data = GetPawnData<UAbilityPawnDataBase>();
		TArray<FAbilityGrantDefinition> Grants;
		for (const FVitalityActionAbilityData& Action : Data.ActionAbilities)
		{
			FAbilityGrantDefinition Grant;
			Grant.DefinitionId = Action.ID;
			Grant.AbilityClass = Action.AbilityClass;
			Grant.SourceObject = const_cast<UAbilityPawnDataBase*>(&Data);
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
		SetNameA(SaveData->Name);
		SetLevelA(SaveData->Level);
		SetRaceID(SaveData->RaceID);

		if (HasAuthority() && SaveData->InventorySnapshot.InventoryId.IsValid())
		{
			UInventoryWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
			UAbilityInventoryBase* Resolved = Subsystem ? Subsystem->Resolve(SaveData->InventorySnapshot.InventoryId) : nullptr;
			if (!Resolved)
			{
				UE_LOG(LogTemp, Error, TEXT("Inventory restore requires the recovered world store: %s"), *SaveData->InventorySnapshot.InventoryId.ToString());
				return;
			}
			BindInventory(Resolved);
		}
	}
}

FParameter AAbilityPawnBase::ToData()
{
	FPawnSaveData SaveData;

	SaveData.ActorID = ActorID;
	SaveData.AssetID = AssetID;
	SaveData.Name = Name;
	SaveData.RaceID = RaceID;
	SaveData.Level = Level;

	if (Inventory)
	{
		SaveData.InventorySnapshot = Inventory->MakeSnapshot();
	}

	SaveData.SpawnTransform = GetActorTransform();
	SaveData.BirthTransform = BirthTransform;



	return FParameter(MoveTemp(SaveData));
}

void AAbilityPawnBase::ResetData()
{
	SetHealth(GetMaxHealth());
}

void AAbilityPawnBase::OnFiniteStateRefresh(UFiniteStateBase* InCurrentState)
{
	if (InCurrentState && ControlFSM->GetCurrentState() == InCurrentState)
	{
		ControlFSM->SwitchState(nullptr);
	}
	else if (!IsDead())
	{
		SwitchFiniteStateByClass<UAbilityPawnState_Walk>();
	}
}

void AAbilityPawnBase::Death(IAbilityVitalityInterface* InKiller)
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	ControlFSM->SwitchState(nullptr);
	const TArray<FAbilityGrantedEntry> Entries = AbilitySystem->GetGrantedEntries();
	for (const FAbilityGrantedEntry& Entry : Entries)
	{
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Entry.AbilityHandle);
		if (UAbilityBase* Action = Spec ? Cast<UAbilityBase>(Spec->GetPrimaryInstance()) : nullptr; Action && Action->IsActive())
		{
			Action->FailAction(AbilityActionTags::Blocked);
		}
	}
	AbilitySystem->RemoveEffectsOnDeath();
	SwitchFinalFiniteState({Cast<UObject>(InKiller)});
}

void AAbilityPawnBase::Kill(IAbilityVitalityInterface* InTarget)
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

void AAbilityPawnBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	if (!HasAuthority() || !IsDead())
	{
		return;
	}

	SwitchDefaultFiniteState({Cast<UObject>(InRescuer)});
}

void AAbilityPawnBase::Static()
{
	if (HasAuthority() && !IsDead())
	{
		ControlFSM->SwitchStateByClass<UAbilityPawnState_Static>();
	}
}

void AAbilityPawnBase::UnStatic()
{
	if (HasAuthority() && ControlFSM->IsCurrentStateClass<UAbilityPawnState_Static>())
	{
		ControlFSM->SwitchState(nullptr);
	}
}

void AAbilityPawnBase::Interrupt(float InDuration)
{
	if (HasAuthority() && !IsDead() && FMath::IsFinite(InDuration))
	{
		ControlFSM->SwitchStateByClass<UAbilityPawnState_Interrupt>({InDuration});
	}
}

void AAbilityPawnBase::UnInterrupt()
{
	if (HasAuthority() && ControlFSM->IsCurrentStateClass<UAbilityPawnState_Interrupt>())
	{
		ControlFSM->SwitchState(nullptr);
	}
}

bool AAbilityPawnBase::DoAction(const FGameplayTag& InActionTag)
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

bool AAbilityPawnBase::StopAction(const FGameplayTag& InActionTag)
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



void AAbilityPawnBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityPawnBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}


void AAbilityPawnBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if (!HasAuthority())
	{
		return;
	}

	if (InAttributeChangeData.Attribute == GetExpAttribute())
	{
		if (GetMaxExp() > UE_SMALL_NUMBER && InAttributeChangeData.NewValue >= GetMaxExp())
		{
			const float Exp = InAttributeChangeData.NewValue - GetMaxExp();
			SetLevelA(GetLevelA() + 1);
			SetExp(Exp);
		}
	}
}

void AAbilityPawnBase::OnActorAttached(AActor* InActor)
{
}

void AAbilityPawnBase::OnActorDetached(AActor* InActor)
{
}

void AAbilityPawnBase::HandleDamage(const FGameplayAttribute& DamageAttribute,
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

void AAbilityPawnBase::HandleRecovery(
    const FGameplayAttribute& RecoveryAttribute, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if (!HasAuthority() || IsDead() || !FMath::IsFinite(RecoveryValue) || RecoveryValue <= 0.f)
	{
		return;
	}

	if (RecoveryAttribute == GetHealthRecoveryAttribute())
	{
		ModifyHealth(RecoveryValue);

		if (RecoveryValue > 1.f)
		{
			USceneModuleStatics::SpawnWorldText(FString::FromInt(RecoveryValue), FColor::Green, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
		}
	}
}

void AAbilityPawnBase::HandleInterrupt(const FGameplayAttribute& InterruptAttribute,
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

UAttributeSetBase* AAbilityPawnBase::GetAttributeSet() const
{
	return AttributeSet;
}

UShapeComponent* AAbilityPawnBase::GetCollisionComponent() const
{
	return BoxComponent;
}

UMeshComponent* AAbilityPawnBase::GetMeshComponent() const
{
	return nullptr;
}

UAbilitySystemComponent* AAbilityPawnBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UInteractionComponent* AAbilityPawnBase::GetInteractionComponent() const
{
	return Interaction;
}

UAbilityInventoryBase* AAbilityPawnBase::GetInventory() const
{
	return Inventory;
}

bool AAbilityPawnBase::IsPlayer() const
{
	return IsPlayerControlled() || GetPlayerState() != nullptr;
}

bool AAbilityPawnBase::IsEnemy(IAbilityPawnInterface* InTarget) const
{
	return InTarget && InTarget != this && !InTarget->GetRaceID().IsEqual(RaceID);
}

bool AAbilityPawnBase::IsActive() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Active);
}

bool AAbilityPawnBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dead) || (bCheckDying && IsDying());
}

bool AAbilityPawnBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Dying);
}

bool AAbilityPawnBase::IsWalking(bool bReally) const
{
	return !IsDead() && !IsInterrupting() && (bReally ? GetMoveVelocity(true).SizeSquared() > 0.04f : IsActive());
}

bool AAbilityPawnBase::IsInterrupting() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

bool AAbilityPawnBase::IsMoving() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Pawn_Moving);
}

bool AAbilityPawnBase::IsTargetAble_Implementation(APawn* InPlayerPawn) const
{
	return !IsDead();
}

bool AAbilityPawnBase::SetLevelA(int32 InLevel)
{
	if (!HasAuthority())
	{
		return false;
	}
	const auto& Data = GetPawnData<UAbilityPawnDataBase>();
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

FString AAbilityPawnBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

float AAbilityPawnBase::GetRadius() const
{
	return FMath::Max(BoxComponent->GetScaledBoxExtent().X, BoxComponent->GetScaledBoxExtent().Y);
}

float AAbilityPawnBase::GetHalfHeight() const
{
	return BoxComponent->GetScaledBoxExtent().Z;
}

float AAbilityPawnBase::GetDistance(AActor* InTargetActor, bool bIgnoreRadius /*= true*/, bool bIgnoreZAxis /*= true*/) const
{
	if (!InTargetActor)
		return -1;

	IAbilityActorInterface* TargetAbilityActor = Cast<IAbilityActorInterface>(InTargetActor);

	return FVector::Distance(
	           FVector(GetActorLocation().X, GetActorLocation().Y, bIgnoreZAxis ? 0 : GetActorLocation().Z),
	           FVector(InTargetActor->GetActorLocation().X, InTargetActor->GetActorLocation().Y, bIgnoreZAxis ? 0 : InTargetActor->GetActorLocation().Z)) -
	       (bIgnoreRadius || !TargetAbilityActor ? 0.f : TargetAbilityActor->GetRadius());
}

void AAbilityPawnBase::GetMotionRate(float& OutMovementRate, float& OutRotationRate)
{
	OutMovementRate = MovementRate;
	OutRotationRate = RotationRate;
}

void AAbilityPawnBase::SetMotionRate(float InMovementRate, float InRotationRate)
{
	MovementRate = InMovementRate;
	RotationRate = InRotationRate;
}

bool AAbilityPawnBase::HasActionAbility(const FGameplayTag& InActionTag) const
{
	FGameplayAbilitySpecHandle Handle;
	return AbilitySystem->FindGrantedAbility(FPrimaryAssetId(), InActionTag, Handle);
}

FVitalityActionAbilityData AAbilityPawnBase::GetActionAbility(const FGameplayTag& InActionTag)
{
	const TMap<FGameplayTag, FVitalityActionAbilityData> Projection = GetActionAbilities();
	const FVitalityActionAbilityData* Data = Projection.Find(InActionTag);
	return Data ? *Data : FVitalityActionAbilityData();
}

void AAbilityPawnBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitializeAbilities();
}

void AAbilityPawnBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilities();

	if (GetPlayerState())
	{
		BindASCInput();
	}
}


TMap<FGameplayTag, FVitalityActionAbilityData> AAbilityPawnBase::GetActionAbilities() const
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

FGuid AAbilityPawnBase::GetDefaultGrantSourceId() const
{
	return FGuid(ActorID.A ^ 0x1D305B4Eu, ActorID.B, ActorID.C, ActorID.D ^ 0x083A49C2u);
}

FGuid AAbilityPawnBase::GetProgressionGrantSourceId() const
{
	return FGuid(ActorID.A ^ 0x6C02753Du, ActorID.B, ActorID.C, ActorID.D ^ 0x732749E1u);
}

void AAbilityPawnBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityPawnBase, RaceID);
	DOREPLIFETIME(AAbilityPawnBase, Level);
}
void AAbilityPawnBase::BindInventory(UAbilityInventoryBase* InInventory)
{
	if (Inventory == InInventory)
	{
		return;
	}
	UInventoryWorldSubsystem* NextSubsystem = InInventory ? Cast<UInventoryWorldSubsystem>(InInventory->GetOuter()) : nullptr;
	UInventoryWorldSubsystem* WorldSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
	if (InInventory && (!NextSubsystem || NextSubsystem != WorldSubsystem || !NextSubsystem->AcquireLease(InInventory->GetInventoryId())))
	{
		UE_LOG(LogTemp, Error, TEXT("Inventory binding rejected for %s"), *GetName());
		return;
	}
	UAbilityInventoryBase* PreviousInventory = Inventory;
	Inventory = InInventory;
	if (PreviousInventory)
	{
		if (UInventoryWorldSubsystem* PreviousSubsystem = Cast<UInventoryWorldSubsystem>(PreviousInventory->GetOuter()))
		{
			PreviousSubsystem->ReleaseLease(PreviousInventory->GetInventoryId());
		}
	}
}
