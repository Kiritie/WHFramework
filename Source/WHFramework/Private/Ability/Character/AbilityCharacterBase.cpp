// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/States/AbilityCharacterState_Alive.h"
#include "Ability/Effects/CharacterStateEffectBase.h"
#include "Character/Components/CharacterMovementComponentBase.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectAggregator.h"
#include "Net/UnrealNetwork.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Engine/World.h"

#include "Ability/Interaction/AbilityInteractionOptions.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Attributes/CharacterAttributeSetBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Ability/Character/States/AbilityCharacterState_Death.h"
#include "Ability/Character/States/AbilityCharacterState_Spawn.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Common/CommonModuleStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/VitalityActionAbilityBase.h"
#include "Common/Looking/LookingComponent.h"
#include "Scene/Actor/PhysicsVolume/PhysicsVolumeBase.h"

//////////////////////////////////////////////////////////////////////////
// AAbilityCharacterBase
AAbilityCharacterBase::AAbilityCharacterBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UCharacterMovementComponentBase>(ACharacter::CharacterMovementComponentName))
{
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCharacterMovement()->bAutoActivate = false;
	GetCharacterMovement()->PrimaryComponentTick.bStartWithTickEnabled = false;
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AbilitySystem = CreateOptionalDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	if (AbilitySystem)
	{
		AbilitySystem->SetIsReplicated(true);
		AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	}

	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSetBase>(FName("AttributeSet"));

	Inventory = nullptr;

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetInteractable(false);

	Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_AbilityRevive>(TEXT("ReviveOption")));

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = FName("Character");

	FSM->DefaultState = UAbilityCharacterState_Spawn::StaticClass();
	FSM->FinalState = UAbilityCharacterState_Death::StaticClass();


	FSM->DomainName = TEXT("Lifecycle");
	FSM->States = {UAbilityCharacterState_Spawn::StaticClass(), UAbilityCharacterState_Alive::StaticClass(), UAbilityCharacterState_Death::StaticClass()};
	bReplicates = true;
	SetReplicateMovement(true);
	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;

	MovementRate = 1.f;
	RotationRate = 1.f;

}

void AAbilityCharacterBase::InitializeAbilities(AActor* InOwnerActor, AActor* InAvatarActor)
{
	IAbilityActorInterface::InitializeAbilities(InOwnerActor, InAvatarActor);
	if (HasAuthority() && AbilitySystem && AbilitySystem->GetAvatarActor() == this && AssetID.IsValid() && !bBaseAttributeSourceReady)
	{
		AAbilityCharacterBase::SetLevelA(Level);
	}
}

void AAbilityCharacterBase::OnSpawn_Implementation(const FParameter& InParam)
{
	bBaseAttributeSourceReady = false;
	Super::OnSpawn_Implementation(InParam);
	InitializeAbilities();
	if (HasAuthority())
	{
		SpawnDefaultController();
		SwitchDefaultFiniteState();
	}
}

void AAbilityCharacterBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	bBaseAttributeSourceReady = false;
	UninitializeAbilities();
	if (UCharacterMovementComponentBase* Movement = Cast<UCharacterMovementComponentBase>(GetCharacterMovement()))
	{
		Movement->ResetMovementSources();
	}
	BindInventory(nullptr);
	SelectedInventorySlot = FInventorySlotKey();
	AssetID = FPrimaryAssetId();
	RaceID = NAME_None;
	Level = 0;
	BirthTransform = FTransform::Identity;
	StaticEffect.Invalidate();
	InterruptEffect.Invalidate();
	Super::OnDespawn_Implementation(InMode);
}

void AAbilityCharacterBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	DefaultGravityScale = GetCharacterMovement()->GravityScale;
	DefaultAirControl = GetCharacterMovement()->AirControl;
}

void AAbilityCharacterBase::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	RefreshAttributes();
}

void AAbilityCharacterBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAbilityCharacterBase::OnTermination_Implementation()
{
	BindInventory(nullptr);
	UninitializeAbilities();
	Super::OnTermination_Implementation();
}

void AAbilityCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BindASCInput();
}

void AAbilityCharacterBase::BindASCInput()
{
	if (!bASCInputBound && AbilitySystem && InputComponent && IsLocallyControlled())
	{
		AbilitySystem->BindAbilityActivationToInputComponent(InputComponent,
			FGameplayAbilityInputBinds(TEXT("ConfirmTarget"), TEXT("CancelTarget"),
				FTopLevelAssetPath(TEXT("/Script/WHFramework"), TEXT("EAbilityInputID")),
				static_cast<int32>(EAbilityInputID::Confirm), static_cast<int32>(EAbilityInputID::Cancel)));
		bASCInputBound = true;
	}
}

void AAbilityCharacterBase::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (IsActive() && !WorldDirection.ContainsNaN() && FMath::IsFinite(ScaleValue))
	{
		Super::AddMovementInput(WorldDirection, FMath::Clamp(ScaleValue, -1.f, 1.f), bForce);
	}
}

void AAbilityCharacterBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void AAbilityCharacterBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	const FCharacterSaveData& SaveData = InSaveData.GetRef<FCharacterSaveData>();
	if (PHASEC(InPhase, EPhase::Primary))
	{
		bBaseAttributeSourceReady = false;
		if (HasAuthority())
		{
			SetActorTransform(SaveData.SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
		BirthTransform = SaveData.IsSaved() ? SaveData.BirthTransform : SaveData.SpawnTransform;
	}
	if (PHASEC(InPhase, EPhase::All))
	{
		SetNameA(SaveData.Name);
		SetRaceID(SaveData.RaceID);
		SetLevelA(SaveData.Level);

		if (HasAuthority() && SaveData.InventorySnapshot.InventoryId.IsValid())
		{
			UInventoryWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
			UAbilityInventoryBase* Resolved = Subsystem ? Subsystem->Resolve(SaveData.InventorySnapshot.InventoryId) : nullptr;
			if (!Resolved)
			{
				UE_LOG(LogTemp, Error, TEXT("Inventory restore requires the recovered world store: %s"), *SaveData.InventorySnapshot.InventoryId.ToString());
				return;
			}
			BindInventory(Resolved);
		}
	}
}

FParameter AAbilityCharacterBase::ToData()
{
	FCharacterSaveData SaveData;
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

void AAbilityCharacterBase::ResetData()
{
	if (HasAuthority() && AbilitySystem)
	{
		SetHealth(GetMaxHealth());
	}
}





void AAbilityCharacterBase::Death(IAbilityVitalityInterface* InKiller)
{
	if (HasAuthority() && !IsDead())
	{
		SwitchFinalFiniteState({Cast<UObject>(InKiller)});
	}
}

void AAbilityCharacterBase::ApplyDeathAttributes()
{
	SetExp(0.f);
	SetHealth(0.f);
}

void AAbilityCharacterBase::Kill(IAbilityVitalityInterface* InTarget)
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

void AAbilityCharacterBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	if (HasAuthority() && IsDead())
	{
		SwitchDefaultFiniteState({Cast<UObject>(InRescuer)});
	}
}

void AAbilityCharacterBase::Static()
{
	if (!AbilitySystem || !HasAuthority() || StaticEffect.IsValid())
	{
		return;
	}
	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UCharacterStateEffectBase::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	Spec.Data->DynamicGrantedTags.AddTag(CharacterLifecycleTags::Suspended);
	StaticEffect = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	FCharacterMovementConstraint Constraint;
	Constraint.bBlockMovement = true;
	Constraint.bBlockRotation = true;
	CastChecked<UCharacterMovementComponentBase>(GetCharacterMovement())->SetMovementConstraint(StaticMovementSource, this, Constraint);
}

void AAbilityCharacterBase::UnStatic()
{
	if (HasAuthority() && AbilitySystem)
	{
		AbilitySystem->RemoveActiveGameplayEffect(StaticEffect);
		StaticEffect.Invalidate();
		CastChecked<UCharacterMovementComponentBase>(GetCharacterMovement())->ReleaseMovementSource(StaticMovementSource, this);
	}
}

void AAbilityCharacterBase::Interrupt(float InDuration /*= -1*/)
{
	if (!HasAuthority() || !AbilitySystem || !FMath::IsFinite(InDuration) || InDuration <= 0.f)
	{
		return;
	}
	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UCharacterTimedStateEffectBase::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	Spec.Data->SetDuration(InDuration, true);
	Spec.Data->DynamicGrantedTags.AddTag(GameplayTags::State_Vitality_Interrupting);
	InterruptEffect = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void AAbilityCharacterBase::UnInterrupt()
{
	if (HasAuthority() && AbilitySystem)
	{
		AbilitySystem->RemoveActiveGameplayEffect(InterruptEffect);
		InterruptEffect.Invalidate();
	}
}

void AAbilityCharacterBase::FreeToAnim()
{
	if (HasAuthority() && AbilitySystem)
	{
		AbilitySystem->SetLooseGameplayTagCount(GameplayTags::State_Character_FreeToAnim, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void AAbilityCharacterBase::LimitToAnim()
{
	if (HasAuthority() && AbilitySystem)
	{
		AbilitySystem->SetLooseGameplayTagCount(GameplayTags::State_Character_FreeToAnim, 0, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void AAbilityCharacterBase::Jump()
{
	if (IsActive() && !IsInterrupting())
	{
		Super::Jump();
	}
}

void AAbilityCharacterBase::UnJump()
{
	Super::StopJumping();
}

void AAbilityCharacterBase::Crouch(bool bClientSimulation)
{
	if (IsActive() || bClientSimulation)
	{
		Super::Crouch(bClientSimulation);
	}
}

void AAbilityCharacterBase::UnCrouch(bool bClientSimulation)
{
	Super::UnCrouch(bClientSimulation);
}









void AAbilityCharacterBase::Climb()
{
	DoAction(GameplayTags::Ability_Character_Action_Climb);
}

void AAbilityCharacterBase::UnClimb()
{
	StopAction(GameplayTags::Ability_Character_Action_Climb);
}

void AAbilityCharacterBase::Fly()
{
	DoAction(GameplayTags::Ability_Character_Action_Fly);
}

void AAbilityCharacterBase::UnFly()
{
	StopAction(GameplayTags::Ability_Character_Action_Fly);
}

bool AAbilityCharacterBase::DoAction(const FGameplayTag& InActionTag)
{
	if (!AbilitySystem || !IsActive())
	{
		return false;
	}
	FAbilityActionRequest Request;
	Request.ActionTag = InActionTag;
	Request.AimDirection = GetBaseAimRotation().Vector();
	return AbilitySystem->RequestAction(Request).bAccepted;
}

bool AAbilityCharacterBase::StopAction(const FGameplayTag& InActionTag)
{
	if (!AbilitySystem)
	{
		return false;
	}
	TArray<FGuid> Requests;
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (UAbilityBase* Ability = Cast<UAbilityBase>(Spec.GetPrimaryInstance()))
		{
			if (Ability->IsActive() && Ability->ActionTag == InActionTag)
			{
				Requests.Add(Ability->GetActionActivationId());
			}
		}
	}
	bool bCancelled = false;
	for (const FGuid& Request : Requests)
	{
		bCancelled |= AbilitySystem->CancelAction(Request, AbilityActionTags::Cancelled);
	}
	return bCancelled;
}


void AAbilityCharacterBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityCharacterBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}


void AAbilityCharacterBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if (!AbilitySystem || !AttributeSet)
	{
		return;
	}
	if (HasAuthority() && InAttributeChangeData.Attribute == GetExpAttribute() && GetMaxExp() > 0.f && InAttributeChangeData.NewValue >= GetMaxExp())
	{
		const float Remaining = InAttributeChangeData.NewValue - GetMaxExp();
		if (SetLevelA(Level + 1))
		{
			SetExp(Remaining);
		}
	}
	CastChecked<UCharacterMovementComponentBase>(GetCharacterMovement())->SetBaseMovementSettings(
		GetMoveSpeed() * MovementRate, GetSwimSpeed() * MovementRate, GetFlySpeed() * MovementRate,
		GetRotationSpeed() * RotationRate, GetJumpForce());
	Looking->LookingRotationSpeed = GetRotationSpeed() * RotationRate;
}

void AAbilityCharacterBase::OnActorAttached(AActor* InActor)
{
}

void AAbilityCharacterBase::OnActorDetached(AActor* InActor)
{
}

void AAbilityCharacterBase::HandleDamage(const FGameplayAttribute& DamageAttribute,
                                         float DamageValue,
                                         float DefendValue,
                                         bool bHasCrited,
                                         const FHitResult& HitResult,
                                         const FGameplayTagContainer& SourceTags,
                                         AActor* SourceActor)
{
	if (!HasAuthority() || IsDead() || !FMath::IsFinite(DamageValue) || DamageValue < 0.f)
	{
		return;
	}
	ModifyHealth(-DamageValue);
	if (GetHealth() <= 0.f)
	{
		if (IAbilityVitalityInterface* Source = Cast<IAbilityVitalityInterface>(SourceActor))
		{
			Source->Kill(this);
		}
		else
		{
			Death(nullptr);
		}
	}
}

void AAbilityCharacterBase::HandleRecovery(
    const FGameplayAttribute& RecoveryAttribute, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if (HasAuthority() && !IsDead() && RecoveryAttribute == GetHealthRecoveryAttribute() && FMath::IsFinite(RecoveryValue))
	{
		ModifyHealth(FMath::Max(0.f, RecoveryValue));
	}
}

void AAbilityCharacterBase::HandleInterrupt(const FGameplayAttribute& InterruptAttribute,
                                            float InterruptDuration,
                                            const FHitResult& HitResult,
                                            const FGameplayTagContainer& SourceTags,
                                            AActor* SourceActor)
{
	if (HasAuthority() && InterruptDuration > 0.f)
	{
		Interrupt(InterruptDuration);
	}
}

UAbilitySystemComponent* AAbilityCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UAttributeSetBase* AAbilityCharacterBase::GetAttributeSet() const
{
	return AttributeSet;
}

UShapeComponent* AAbilityCharacterBase::GetCollisionComponent() const
{
	return GetCapsuleComponent();
}

UMeshComponent* AAbilityCharacterBase::GetMeshComponent() const
{
	return GetMesh();
}

UInteractionComponent* AAbilityCharacterBase::GetInteractionComponent() const
{
	return Interaction;
}

UAbilityInventoryBase* AAbilityCharacterBase::GetInventory() const
{
	return Inventory;
}

bool AAbilityCharacterBase::IsPlayer() const
{
	return GetPlayerState() != nullptr || IsPlayerControlled();
}

bool AAbilityCharacterBase::IsEnemy(IAbilityPawnInterface* InTarget) const
{
	return InTarget && InTarget != this && InTarget->GetRaceID() != RaceID;
}

bool AAbilityCharacterBase::IsActive() const
{
	return FSM && FSM->GetCurrentState() && FSM->GetCurrentState()->GetStateName() == TEXT("Alive")
		&& AbilitySystem && !AbilitySystem->HasMatchingGameplayTag(CharacterLifecycleTags::Suspended);
}

bool AAbilityCharacterBase::IsDead(bool bCheckDying) const
{
	return FSM && FSM->GetCurrentState() && FSM->GetCurrentState()->GetStateName() == TEXT("Death") && (bCheckDying || !IsDying());
}

bool AAbilityCharacterBase::IsDying() const
{
	const UAbilityCharacterState_Death* State = FSM ? Cast<UAbilityCharacterState_Death>(FSM->GetCurrentState()) : nullptr;
	return State && !State->IsDeathPresentationFinished();
}

bool AAbilityCharacterBase::IsWalking(bool bReally) const
{
	return GetCharacterMovement()->IsMovingOnGround();
}

bool AAbilityCharacterBase::IsInterrupting() const
{
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

bool AAbilityCharacterBase::IsMoving() const
{
	return GetVelocity().SizeSquared() > 1.f;
}

bool AAbilityCharacterBase::IsFreeToAnim() const
{
	return IsActive() && !IsInterrupting() && !IsAnimating();
}

bool AAbilityCharacterBase::IsAnimating() const
{
	return GetMesh() && GetMesh()->GetAnimInstance() && GetMesh()->GetAnimInstance()->IsAnyMontagePlaying();
}

bool AAbilityCharacterBase::IsJumping() const
{
	return GetCharacterMovement()->IsFalling() && GetVelocity().Z > 0.f;
}

bool AAbilityCharacterBase::IsFalling(bool bReally) const
{
	return GetCharacterMovement()->IsFalling();
}

bool AAbilityCharacterBase::IsCrouching(bool bReally) const
{
	return GetCharacterMovement()->IsCrouching();
}

bool AAbilityCharacterBase::IsSwimming(bool bReally) const
{
	return GetCharacterMovement()->IsSwimming();
}

bool AAbilityCharacterBase::IsFloating() const
{
	return CastChecked<UCharacterMovementComponentBase>(GetCharacterMovement())->IsFloating();
}

bool AAbilityCharacterBase::IsClimbing() const
{
	return CastChecked<UCharacterMovementComponentBase>(GetCharacterMovement())->IsClimbing();
}

bool AAbilityCharacterBase::IsFlying(bool bReally) const
{
	return GetCharacterMovement()->IsFlying();
}

bool AAbilityCharacterBase::IsTargetAble_Implementation(APawn* InPlayerPawn) const
{
	return !IsDead();
}

bool AAbilityCharacterBase::IsLookAtAble_Implementation(AActor* InLookerActor) const
{
	return !IsDead();
}

bool AAbilityCharacterBase::CanLookAtTarget()
{
	return Super::CanLookAtTarget() && IsActive();
}

void AAbilityCharacterBase::SetNameA(FName InName)
{
	Name = InName;
}

void AAbilityCharacterBase::SetRaceID(FName InRaceID)
{
	RaceID = InRaceID;
}

bool AAbilityCharacterBase::SetLevelA(int32 InLevel)
{
	if (!HasAuthority() || !AssetID.IsValid())
	{
		return false;
	}
	const UAbilityCharacterDataBase& Data = GetCharacterData<UAbilityCharacterDataBase>();
	const int32 NewLevel = Data.ClampLevel(InLevel);
	const bool bChanged = Level != NewLevel;
	const int32 PreviousLevel = Level;
	const bool bWasReady = bBaseAttributeSourceReady;
	FScopedAggregatorOnDirtyBatch AttributeBatch;
	Level = NewLevel;
	if (!AbilitySystem)
	{
		bBaseAttributeSourceReady = false;
		return bChanged;
	}
	TArray<TSubclassOf<UEffectBase>> BaseEffects;
	if (Data.PEClass)
	{
		BaseEffects.Add(Data.PEClass);
	}
	if (!AbilitySystem->SetGrantSource(BaseStatsSource, {}, BaseEffects, NewLevel))
	{
		Level = PreviousLevel;
		bBaseAttributeSourceReady = bWasReady;
		UE_LOG(LogTemp, Error, TEXT("角色基础属性来源建立失败: %s"), *AssetID.ToString());
		return false;
	}
	bBaseAttributeSourceReady = true;
	return bChanged;
}

FString AAbilityCharacterBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

float AAbilityCharacterBase::GetRadius() const
{
	if (!GetCapsuleComponent())
		return 0.f;

	return GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float AAbilityCharacterBase::GetHalfHeight() const
{
	if (!GetCapsuleComponent())
		return 0.f;

	return GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

float AAbilityCharacterBase::GetDistance(AActor* InTargetActor, bool bIgnoreRadius /*= true*/, bool bIgnoreZAxis /*= true*/) const
{
	if (!InTargetActor)
		return -1;

	IAbilityActorInterface* TargetAbilityActor = Cast<IAbilityActorInterface>(InTargetActor);

	return FVector::Distance(
	           FVector(GetActorLocation().X, GetActorLocation().Y, bIgnoreZAxis ? 0 : GetActorLocation().Z),
	           FVector(InTargetActor->GetActorLocation().X, InTargetActor->GetActorLocation().Y, bIgnoreZAxis ? 0 : InTargetActor->GetActorLocation().Z)) -
	       (bIgnoreRadius || !TargetAbilityActor ? 0.f : TargetAbilityActor->GetRadius());
}

void AAbilityCharacterBase::GetMotionRate(float& OutMovementRate, float& OutRotationRate)
{
	OutMovementRate = MovementRate;
	OutRotationRate = RotationRate;
}

void AAbilityCharacterBase::SetMotionRate(float InMovementRate, float InRotationRate)
{
	MovementRate = FMath::IsFinite(InMovementRate) ? FMath::Clamp(InMovementRate, 0.f, 10.f) : 1.f;
	RotationRate = FMath::IsFinite(InRotationRate) ? FMath::Clamp(InRotationRate, 0.f, 10.f) : 1.f;
	if (AbilitySystem && AttributeSet)
	{
		CastChecked<UCharacterMovementComponentBase>(GetCharacterMovement())->SetBaseMovementSettings(
			GetMoveSpeed() * MovementRate, GetSwimSpeed() * MovementRate, GetFlySpeed() * MovementRate,
			GetRotationSpeed() * RotationRate, GetJumpForce());
	}
}

bool AAbilityCharacterBase::HasActionAbility(const FGameplayTag& InActionTag) const
{
	FGameplayAbilitySpecHandle Handle;
	return AbilitySystem && AbilitySystem->FindGrantedAbility(FPrimaryAssetId(), InActionTag, Handle);
}

FVitalityActionAbilityData AAbilityCharacterBase::GetActionAbility(const FGameplayTag& InActionTag)
{
	const TMap<FGameplayTag, FVitalityActionAbilityData> Definitions = GetActionAbilities();
	const FVitalityActionAbilityData* Found = Definitions.Find(InActionTag);
	return Found ? *Found : FVitalityActionAbilityData();
}

void AAbilityCharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();
}

void AAbilityCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (GetPlayerState())
	{
		BindASCInput();
	}
}


TMap<FGameplayTag, FVitalityActionAbilityData> AAbilityCharacterBase::GetActionAbilities() const
{
	TMap<FGameplayTag, FVitalityActionAbilityData> Result;
	if (!AbilitySystem)
	{
		return Result;
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		const UAbilityBase* Ability = Cast<UAbilityBase>(Spec.Ability);
		if (!Ability || !Ability->ActionTag.IsValid())
		{
			continue;
		}
		FVitalityActionAbilityData Data;
		Data.AbilityClass = Ability->GetClass();
		Data.AbilityHandle = Spec.Handle;
		Data.Level = Spec.Level;
		Result.Add(Ability->ActionTag, Data);
	}
	return Result;
}

void AAbilityCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityCharacterBase, Level);
	DOREPLIFETIME(AAbilityCharacterBase, RaceID);
	DOREPLIFETIME_CONDITION(AAbilityCharacterBase, SelectedInventorySlot, COND_OwnerOnly);
}

bool AAbilityCharacterBase::IsAbilityRuntimeReady() const
{
	return AbilitySystem && AbilitySystem->GetAvatarActor() == this && AttributeSet
		&& (!HasAuthority() || bBaseAttributeSourceReady);
}

bool AAbilityCharacterBase::IsMovementExternallyManaged() const
{
	return false;
}

void AAbilityCharacterBase::OnLifecycleActivated()
{
	if (HasAuthority() && AbilitySystem)
	{
		for (FGameplayTag Tag : {GameplayTags::State_Vitality_Dead.GetTag(), GameplayTags::State_Vitality_Dying.GetTag()})
		{
			AbilitySystem->SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::TagAndCountToAll);
		}
		AbilitySystem->SetLooseGameplayTagCount(GameplayTags::State_Vitality_Active, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	if (!IsMovementExternallyManaged())
	{
		SetActorEnableCollision(true);
		GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	Interaction->SetInteractable(true);
	FreeToAnim();
}

void AAbilityCharacterBase::OnLifecycleSuspended(bool bDeath)
{
	if (HasAuthority() && AbilitySystem)
	{
		AbilitySystem->SetLooseGameplayTagCount(GameplayTags::State_Vitality_Active, 0, EGameplayTagReplicationState::TagAndCountToAll);
		TArray<TWeakObjectPtr<UAbilityBase>> Actions;
		for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (UAbilityBase* Ability = Cast<UAbilityBase>(Spec.GetPrimaryInstance()); Ability && Ability->IsActive())
			{
				Actions.Add(Ability);
			}
		}
		for (const TWeakObjectPtr<UAbilityBase>& Action : Actions)
		{
			if (Action.IsValid())
			{
				Action->FailAction(bDeath ? GameplayTags::State_Vitality_Dead.GetTag() : AbilityActionTags::AvatarRemoved.GetTag());
			}
		}
	}
	Interaction->SetInteractable(false);
	Looking->TargetLookingOff();
	CastChecked<UCharacterMovementComponentBase>(GetCharacterMovement())->ResetMovementSources();
}

void AAbilityCharacterBase::BindInventory(UAbilityInventoryBase* InInventory)
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
	SetSelectedInventorySlot(FInventorySlotKey());
	if (PreviousInventory)
	{
		if (UInventoryWorldSubsystem* PreviousSubsystem = Cast<UInventoryWorldSubsystem>(PreviousInventory->GetOuter()))
		{
			PreviousSubsystem->ReleaseLease(PreviousInventory->GetInventoryId());
		}
	}
}

void AAbilityCharacterBase::SetSelectedInventorySlot(const FInventorySlotKey& InSlotKey)
{
	if (!HasAuthority() || SelectedInventorySlot == InSlotKey)
	{
		return;
	}
	if (InSlotKey.IsValid() && (!Inventory || !Inventory->MakeSnapshot().FindSlot(InSlotKey)))
	{
		return;
	}
	SelectedInventorySlot = InSlotKey;
	OnInventorySelectionUpdated(SelectedInventorySlot);
	ForceNetUpdate();
}

void AAbilityCharacterBase::OnRep_SelectedInventorySlot()
{
	OnInventorySelectionUpdated(SelectedInventorySlot);
}

void AAbilityCharacterBase::OnInventorySelectionUpdated(const FInventorySlotKey& InSlotKey)
{
	OnInventorySelectionChanged.Broadcast(InSlotKey);
}
