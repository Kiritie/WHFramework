#include "Character/Components/CharacterMovementComponentBase.h"

#include "Character/Components/CharacterMovementEnvironmentInterface.h"
#include "Components/CapsuleComponent.h"
#include "Engine/ScopedMovementUpdate.h"
#include "GameFramework/Character.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/RootMotionSource.h"
#include "Net/UnrealNetwork.h"

class FSavedMove_CharacterBase final : public FSavedMove_Character
{
public:
	FCharacterMovementConstraint Constraint;

	virtual void Clear() override
	{
		Super::Clear();
		Constraint = FCharacterMovementConstraint();
	}

	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Flags = Super::GetCompressedFlags();
		if (Constraint.bSprint)
		{
			Flags |= FLAG_Custom_0;
		}
		if (Constraint.Traversal == ECharacterTraversalMode::Climb)
		{
			Flags |= FLAG_Custom_1;
		}
		if (Constraint.Traversal == ECharacterTraversalMode::Fly)
		{
			Flags |= FLAG_Custom_2;
		}
		return Flags;
	}

	virtual bool CanCombineWith(const FSavedMovePtr& InMove, ACharacter* InCharacter, float InMaxDelta) const override
	{
		const FSavedMove_CharacterBase* Other = static_cast<const FSavedMove_CharacterBase*>(InMove.Get());
		return Other && Constraint.Matches(Other->Constraint) && Super::CanCombineWith(InMove, InCharacter, InMaxDelta);
	}

	virtual void SetMoveFor(ACharacter* InCharacter, float InDeltaSeconds, const FVector& InAcceleration, FNetworkPredictionData_Client_Character& InClientData) override
	{
		Super::SetMoveFor(InCharacter, InDeltaSeconds, InAcceleration, InClientData);
		const UCharacterMovementComponentBase* Movement = CastChecked<UCharacterMovementComponentBase>(InCharacter->GetCharacterMovement());
		Constraint = Movement->CaptureMovementConstraint();
	}

	virtual void PrepMoveFor(ACharacter* InCharacter) override
	{
		Super::PrepMoveFor(InCharacter);
		UCharacterMovementComponentBase* Movement = CastChecked<UCharacterMovementComponentBase>(InCharacter->GetCharacterMovement());
		Movement->ReplayConstraint = Constraint;
		Movement->bUsingReplayConstraint = true;
	}

private:
	using Super = FSavedMove_Character;
};

class FNetworkPredictionData_Client_CharacterBase final : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_CharacterBase(const UCharacterMovementComponent& InMovement)
		: FNetworkPredictionData_Client_Character(InMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_CharacterBase());
	}
};

UCharacterMovementComponentBase::UCharacterMovementComponentBase(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	NavAgentProps.bCanSwim = true;
	NavAgentProps.bCanFly = true;
	bUseControllerDesiredRotation = false;
	bOrientRotationToMovement = true;
}

void UCharacterMovementComponentBase::EndPlay(const EEndPlayReason::Type InReason)
{
	ResetMovementSources();
	EnvironmentProvider.Reset();
	OnMovementLanded.Clear();
	OnWaterStateChanged.Clear();
	Super::EndPlay(InReason);
}

void UCharacterMovementComponentBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCharacterMovementComponentBase, AggregatedConstraint, COND_SimulatedOnly);
}

FNetworkPredictionData_Client* UCharacterMovementComponentBase::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UCharacterMovementComponentBase* MutableThis = const_cast<UCharacterMovementComponentBase*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_CharacterBase(*this);
	}
	return ClientPredictionData;
}

bool UCharacterMovementComponentBase::SetEnvironmentProvider(UObject* InProvider)
{
	if (InProvider && (InProvider->GetWorld() != GetWorld() || !Cast<ICharacterMovementEnvironmentInterface>(InProvider)))
	{
		return false;
	}
	EnvironmentProvider = InProvider;
	SetMovementEnvironment(FCharacterMovementEnvironment());
	ObservedWorldEpoch = 0;
	ResetFallTracking();
	return true;
}

bool UCharacterMovementComponentBase::CanAcceptSource(const FGuid& InSourceId, const UObject* InSource) const
{
	if (!InSourceId.IsValid() || !IsValid(InSource) || !CharacterOwner || InSource->GetWorld() != GetWorld())
	{
		return false;
	}
	if (!CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled())
	{
		return false;
	}
	const FMovementSource* Existing = Sources.Find(InSourceId);
	return Existing ? Existing->Owner.Get() == InSource : Sources.Num() < 64;
}

bool UCharacterMovementComponentBase::SetMovementConstraint(const FGuid& InSourceId, UObject* InSource, const FCharacterMovementConstraint& InConstraint)
{
	if (!CanAcceptSource(InSourceId, InSource) || !InConstraint.IsValid())
	{
		return false;
	}
	FMovementSource& Entry = Sources.FindOrAdd(InSourceId);
	Entry.Owner = InSource;
	Entry.Constraint = InConstraint;
	RebuildConstraint();
	return true;
}

bool UCharacterMovementComponentBase::ApplyMovementForce(const FGuid& InSourceId, UObject* InSource, const FVector& InVelocity, float InDuration, bool bInAdditive)
{
	if (!CanAcceptSource(InSourceId, InSource) || InVelocity.ContainsNaN() || !FMath::IsFinite(InDuration) ||
		InDuration <= 0.f || InDuration > 10.f || InVelocity.SizeSquared() > FMath::Square(MaximumForceSpeed))
	{
		return false;
	}
	FMovementSource& Entry = Sources.FindOrAdd(InSourceId);
	Entry.Owner = InSource;
	if (Entry.RootMotionId != 0)
	{
		RemoveRootMotionSourceByID(Entry.RootMotionId);
	}
	TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
	Force->InstanceName = FName(*InSourceId.ToString(EGuidFormats::Digits));
	Force->Priority = bInAdditive ? 100 : 500;
	Force->AccumulateMode = bInAdditive ? ERootMotionAccumulateMode::Additive : ERootMotionAccumulateMode::Override;
	Force->Force = InVelocity;
	Force->Duration = InDuration;
	Force->bInLocalSpace = false;
	Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Force->FinishVelocityParams.ClampVelocity = GetMaxSpeed();
	Entry.RootMotionId = ApplyRootMotionSource(Force);
	return Entry.RootMotionId != 0;
}

bool UCharacterMovementComponentBase::ReleaseMovementSource(const FGuid& InSourceId, UObject* InSource)
{
	FMovementSource* Entry = Sources.Find(InSourceId);
	if (!Entry || Entry->Owner.Get() != InSource)
	{
		return false;
	}
	if (Entry->RootMotionId != 0)
	{
		RemoveRootMotionSourceByID(Entry->RootMotionId);
	}
	Sources.Remove(InSourceId);
	RebuildConstraint();
	return true;
}

void UCharacterMovementComponentBase::ResetMovementSources()
{
	for (const TPair<FGuid, FMovementSource>& Entry : Sources)
	{
		if (Entry.Value.RootMotionId != 0)
		{
			RemoveRootMotionSourceByID(Entry.Value.RootMotionId);
		}
	}
	Sources.Reset();
	AggregatedConstraint = FCharacterMovementConstraint();
	ReplayConstraint = FCharacterMovementConstraint();
	bUsingReplayConstraint = false;
	bNetworkSprintIntent = false;
	bNetworkClimbIntent = false;
	bNetworkFlyIntent = false;
	ResetFallTracking();
}

void UCharacterMovementComponentBase::PruneMovementSources()
{
	bool bChanged = false;
	for (auto Iterator = Sources.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Value().Owner.IsValid())
		{
			if (Iterator.Value().RootMotionId != 0)
			{
				RemoveRootMotionSourceByID(Iterator.Value().RootMotionId);
			}
			Iterator.RemoveCurrent();
			bChanged = true;
		}
	}
	if (bChanged)
	{
		RebuildConstraint();
	}
}

void UCharacterMovementComponentBase::RebuildConstraint()
{
	AggregatedConstraint = FCharacterMovementConstraint();
	TArray<FGuid> Keys;
	Sources.GenerateKeyArray(Keys);
	Keys.Sort([](const FGuid& Left, const FGuid& Right)
	{
		if (Left.A != Right.A) return Left.A < Right.A;
		if (Left.B != Right.B) return Left.B < Right.B;
		if (Left.C != Right.C) return Left.C < Right.C;
		return Left.D < Right.D;
	});
	int32 TraversalPriority = MIN_int32;
	for (const FGuid& Key : Keys)
	{
		const FCharacterMovementConstraint& Constraint = Sources.FindChecked(Key).Constraint;
		AggregatedConstraint.SpeedMultiplier *= Constraint.SpeedMultiplier;
		AggregatedConstraint.RotationMultiplier *= Constraint.RotationMultiplier;
		AggregatedConstraint.bBlockMovement |= Constraint.bBlockMovement;
		AggregatedConstraint.bBlockRotation |= Constraint.bBlockRotation;
		AggregatedConstraint.bSprint |= Constraint.bSprint;
		if (Constraint.Traversal != ECharacterTraversalMode::None && Constraint.Priority >= TraversalPriority)
		{
			AggregatedConstraint.Traversal = Constraint.Traversal;
			TraversalPriority = Constraint.Priority;
		}
	}
	AggregatedConstraint.SpeedMultiplier = FMath::Clamp(AggregatedConstraint.SpeedMultiplier, 0.f, 3.f);
	AggregatedConstraint.RotationMultiplier = FMath::Clamp(AggregatedConstraint.RotationMultiplier, 0.f, 3.f);
}

FCharacterMovementConstraint UCharacterMovementComponentBase::CaptureMovementConstraint() const
{
	return AggregatedConstraint;
}

FCharacterMovementConstraint UCharacterMovementComponentBase::GetSimulationConstraint() const
{
	if (bUsingReplayConstraint && CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		return ReplayConstraint;
	}
	FCharacterMovementConstraint Result = AggregatedConstraint;
	if (CharacterOwner && CharacterOwner->HasAuthority() && CharacterOwner->IsPlayerControlled() && !CharacterOwner->IsLocallyControlled())
	{
		Result.bSprint &= bNetworkSprintIntent;
		if ((Result.Traversal == ECharacterTraversalMode::Climb && !bNetworkClimbIntent) ||
			(Result.Traversal == ECharacterTraversalMode::Fly && !bNetworkFlyIntent))
		{
			Result.Traversal = ECharacterTraversalMode::None;
		}
	}
	return Result;
}

const FCharacterMovementConstraint& UCharacterMovementComponentBase::GetMovementConstraint() const
{
	return AggregatedConstraint;
}

float UCharacterMovementComponentBase::GetMaxSpeed() const
{
	const FCharacterMovementConstraint Constraint = GetSimulationConstraint();
	if (Constraint.bBlockMovement)
	{
		return 0.f;
	}
	const float BaseSpeed = IsClimbing() ? ClimbSpeed : (IsFloating() ? MaxSwimSpeed : Super::GetMaxSpeed());
	return BaseSpeed * Constraint.SpeedMultiplier * (Constraint.bSprint ? SprintMultiplier : 1.f);
}

float UCharacterMovementComponentBase::GetMaxAcceleration() const
{
	return GetSimulationConstraint().bBlockMovement ? 0.f : Super::GetMaxAcceleration();
}

void UCharacterMovementComponentBase::SetBaseMovementSettings(float InWalkSpeed, float InSwimSpeed, float InFlySpeed, float InRotationSpeed, float InJumpVelocity)
{
	if (!FMath::IsFinite(InWalkSpeed) || !FMath::IsFinite(InSwimSpeed) || !FMath::IsFinite(InFlySpeed) ||
		!FMath::IsFinite(InRotationSpeed) || !FMath::IsFinite(InJumpVelocity))
	{
		return;
	}
	MaxWalkSpeed = FMath::Max(0.f, InWalkSpeed);
	MaxWalkSpeedCrouched = MaxWalkSpeed * 0.5f;
	MaxSwimSpeed = FMath::Max(0.f, InSwimSpeed);
	MaxFlySpeed = FMath::Max(0.f, InFlySpeed);
	BaseRotationSpeed = FMath::Max(0.f, InRotationSpeed);
	RotationRate = FRotator(0.f, BaseRotationSpeed, 0.f);
	JumpZVelocity = FMath::Max(0.f, InJumpVelocity);
}

void UCharacterMovementComponentBase::UpdateFromCompressedFlags(uint8 InFlags)
{
	Super::UpdateFromCompressedFlags(InFlags);
	bNetworkSprintIntent = (InFlags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bNetworkClimbIntent = (InFlags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bNetworkFlyIntent = (InFlags & FSavedMove_Character::FLAG_Custom_2) != 0;
}

ICharacterMovementEnvironmentInterface* UCharacterMovementComponentBase::GetEnvironmentProvider() const
{
	return Cast<ICharacterMovementEnvironmentInterface>(EnvironmentProvider.Get());
}

void UCharacterMovementComponentBase::RefreshEnvironment()
{
	if (ICharacterMovementEnvironmentInterface* Provider = GetEnvironmentProvider())
	{
		const FCharacterMovementEnvironment Sample = Provider->SampleMovementEnvironment(UpdatedComponent->GetComponentLocation());
		bWaitingForTerrain = Sample.Result != ECharacterWorldQueryResult::Ready;
		if (Sample.WorldEpoch != 0 && Sample.WorldEpoch != ObservedWorldEpoch)
		{
			ObservedWorldEpoch = Sample.WorldEpoch;
			SetMovementEnvironment(FCharacterMovementEnvironment());
			ResetFallTracking();
		}
		if (!bWaitingForTerrain)
		{
			SetMovementEnvironment(Sample);
			if (ConfiguredStepHeight < 0.f)
			{
				ConfiguredStepHeight = MaxStepHeight;
			}
			MaxStepHeight = Sample.StepHeight >= 0.f ? Sample.StepHeight : ConfiguredStepHeight;
		}
	}
}

void UCharacterMovementComponentBase::SetMovementEnvironment(const FCharacterMovementEnvironment& InEnvironment)
{
	const bool bWaterChanged = Environment.bHeadUnderWater != InEnvironment.bHeadUnderWater || Environment.bBodyInWater != InEnvironment.bBodyInWater;
	Environment = InEnvironment;
	if (bWaterChanged)
	{
		OnWaterStateChanged.Broadcast();
	}
}

void UCharacterMovementComponentBase::SelectEnvironmentMovement()
{
	if (bWaitingForTerrain || MovementMode == MOVE_None)
	{
		return;
	}
	const FCharacterMovementConstraint Constraint = GetSimulationConstraint();
	if (Constraint.Traversal == ECharacterTraversalMode::Climb && Environment.bClimbable)
	{
		SetMovementMode(MOVE_Custom, static_cast<uint8>(ECharacterCustomMovementMode::Climb));
		return;
	}
	if (Constraint.Traversal == ECharacterTraversalMode::Fly)
	{
		SetMovementMode(MOVE_Flying);
		return;
	}
	if (Environment.bHeadUnderWater)
	{
		SetMovementMode(MOVE_Swimming);
		return;
	}
	if (Environment.bBodyInWater && (IsFloating() || MovementMode == MOVE_Swimming || Velocity.Z <= 0.f))
	{
		SetMovementMode(MOVE_Custom, static_cast<uint8>(ECharacterCustomMovementMode::Float));
		return;
	}
	if (MovementMode == MOVE_Swimming || MovementMode == MOVE_Flying || IsClimbing() || IsFloating())
	{
		SetMovementMode(MOVE_Falling);
	}
}

ECharacterWorldQueryResult UCharacterMovementComponentBase::ResumeMovementFromLifecycle()
{
	if (!HasValidData() || !UpdatedPrimitive || !UpdatedPrimitive->IsQueryCollisionEnabled() ||
		!CharacterOwner->GetActorEnableCollision() || CharacterOwner->GetAttachParentActor())
	{
		return ECharacterWorldQueryResult::Invalid;
	}
	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return ECharacterWorldQueryResult::Ready;
	}
	ICharacterMovementEnvironmentInterface* Provider = GetEnvironmentProvider();
	if (Provider)
	{
		RefreshEnvironment();
		if (bWaitingForTerrain)
		{
			return ECharacterWorldQueryResult::WaitingTerrain;
		}
	}
	StopMovementImmediately();
	ResetFallTracking();
	const FCharacterMovementConstraint Constraint = GetSimulationConstraint();
	if (Provider && Constraint.Traversal == ECharacterTraversalMode::Climb && Environment.bClimbable)
	{
		SetMovementMode(MOVE_Custom, static_cast<uint8>(ECharacterCustomMovementMode::Climb));
	}
	else if (Constraint.Traversal == ECharacterTraversalMode::Fly)
	{
		SetMovementMode(MOVE_Flying);
	}
	else if ((Provider && Environment.bHeadUnderWater) || (!Provider && GetPhysicsVolume() && GetPhysicsVolume()->bWaterVolume))
	{
		SetMovementMode(MOVE_Swimming);
	}
	else if (Provider && Environment.bBodyInWater)
	{
		SetMovementMode(MOVE_Custom, static_cast<uint8>(ECharacterCustomMovementMode::Float));
	}
	else
	{
		FFindFloorResult Floor;
		FindFloor(UpdatedComponent->GetComponentLocation(), Floor, false);
		CurrentFloor = Floor;
		SetMovementMode(Floor.IsWalkableFloor() ? MOVE_Walking : MOVE_Falling);
		if (IsMovingOnGround())
		{
			SetBaseFromFloor(CurrentFloor);
		}
	}
	return ECharacterWorldQueryResult::Ready;
}

void UCharacterMovementComponentBase::UpdateCharacterStateBeforeMovement(float InDeltaSeconds)
{
	PruneMovementSources();
	Super::UpdateCharacterStateBeforeMovement(InDeltaSeconds);
	if (!HasValidData() || CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	RefreshEnvironment();
	if (GetEnvironmentProvider())
	{
		SelectEnvironmentMovement();
	}
	if (GetSimulationConstraint().bBlockMovement || bWaitingForTerrain)
	{
		Acceleration = FVector::ZeroVector;
	}
	if (bTrackingFall)
	{
		FallApexZ = FMath::Max(FallApexZ, UpdatedComponent->GetComponentLocation().Z);
	}
}

void UCharacterMovementComponentBase::OnMovementUpdated(float InDeltaSeconds, const FVector& InOldLocation, const FVector& InOldVelocity)
{
	Super::OnMovementUpdated(InDeltaSeconds, InOldLocation, InOldVelocity);
	if (bTrackingFall && UpdatedComponent)
	{
		FallApexZ = FMath::Max(FallApexZ, UpdatedComponent->GetComponentLocation().Z);
	}
	bUsingReplayConstraint = false;
}

void UCharacterMovementComponentBase::PhysicsVolumeChanged(APhysicsVolume* InNewVolume)
{
	if (!GetEnvironmentProvider())
	{
		Super::PhysicsVolumeChanged(InNewVolume);
	}
}

void UCharacterMovementComponentBase::OnMovementModeChanged(EMovementMode InPreviousMode, uint8 InPreviousCustomMode)
{
	Super::OnMovementModeChanged(InPreviousMode, InPreviousCustomMode);
	if (!UpdatedComponent)
	{
		return;
	}
	if (MovementMode == MOVE_Falling && InPreviousMode != MOVE_Falling)
	{
		bTrackingFall = true;
		FallApexZ = UpdatedComponent->GetComponentLocation().Z;
	}
	else if (MovementMode == MOVE_Swimming || IsFloating() || IsClimbing() || MovementMode == MOVE_Flying || MovementMode == MOVE_None)
	{
		ResetFallTracking();
	}
}

void UCharacterMovementComponentBase::ResetFallTracking()
{
	bTrackingFall = false;
	FallApexZ = UpdatedComponent ? UpdatedComponent->GetComponentLocation().Z : 0.0;
}

void UCharacterMovementComponentBase::PhysFalling(float InDeltaSeconds, int32 InIterations)
{
	if (bWaitingForTerrain)
	{
		return;
	}
	if (!bTrackingFall && UpdatedComponent)
	{
		bTrackingFall = true;
		FallApexZ = UpdatedComponent->GetComponentLocation().Z;
	}
	if (bTrackingFall && UpdatedComponent)
	{
		FallApexZ = FMath::Max(FallApexZ, UpdatedComponent->GetComponentLocation().Z);
	}
	const FVector PreviousVelocity = Velocity;
	Super::PhysFalling(InDeltaSeconds, InIterations);
	if (bWaitingForTerrain)
	{
		Velocity = PreviousVelocity;
	}
}

void UCharacterMovementComponentBase::ProcessLanded(const FHitResult& InHit, float InRemainingTime, int32 InIterations)
{
	if (bWaitingForTerrain && !InHit.GetComponent())
	{
		return;
	}
	const float FallDistance = bTrackingFall && UpdatedComponent ? static_cast<float>(FMath::Max(0.0, FallApexZ - UpdatedComponent->GetComponentLocation().Z)) : 0.f;
	const float DownwardSpeed = FMath::Max(0.f, static_cast<float>(-Velocity.Z));
	const bool bNotify = bTrackingFall && CharacterOwner && CharacterOwner->HasAuthority();
	ResetFallTracking();
	Super::ProcessLanded(InHit, InRemainingTime, InIterations);
	if (bNotify)
	{
		OnMovementLanded.Broadcast(InHit, FallDistance, DownwardSpeed);
	}
}

void UCharacterMovementComponentBase::PhysicsRotation(float InDeltaSeconds)
{
	const FCharacterMovementConstraint Constraint = GetSimulationConstraint();
	if (Constraint.bBlockRotation)
	{
		return;
	}
	RotationRate.Yaw = BaseRotationSpeed * Constraint.RotationMultiplier;
	if (IsClimbing() && Environment.bClimbable)
	{
		const FRotator Desired = (-Environment.ClimbNormal).Rotation();
		const FRotator Current = UpdatedComponent->GetComponentRotation();
		const FRotator Rotation(0.f, FMath::FixedTurn(Current.Yaw, Desired.Yaw, RotationRate.Yaw * InDeltaSeconds), 0.f);
		MoveUpdatedComponent(FVector::ZeroVector, Rotation, false);
		return;
	}
	Super::PhysicsRotation(InDeltaSeconds);
}

void UCharacterMovementComponentBase::PhysCustom(float InDeltaSeconds, int32 InIterations)
{
	if (IsClimbing())
	{
		PhysClimbing(InDeltaSeconds, InIterations);
	}
	else if (IsFloating())
	{
		PhysWater(InDeltaSeconds, InIterations, true);
	}
	else
	{
		Super::PhysCustom(InDeltaSeconds, InIterations);
	}
}

void UCharacterMovementComponentBase::PhysSwimming(float InDeltaSeconds, int32 InIterations)
{
	if (GetEnvironmentProvider())
	{
		PhysWater(InDeltaSeconds, InIterations, false);
	}
	else
	{
		Super::PhysSwimming(InDeltaSeconds, InIterations);
	}
}

void UCharacterMovementComponentBase::PhysFlying(float InDeltaSeconds, int32 InIterations)
{
	if (!bWaitingForTerrain)
	{
		Super::PhysFlying(InDeltaSeconds, InIterations);
	}
}

void UCharacterMovementComponentBase::PhysWalking(float InDeltaSeconds, int32 InIterations)
{
	if (!bWaitingForTerrain)
	{
		Super::PhysWalking(InDeltaSeconds, InIterations);
	}
}

void UCharacterMovementComponentBase::PhysNavWalking(float InDeltaSeconds, int32 InIterations)
{
	if (!bWaitingForTerrain)
	{
		Super::PhysNavWalking(InDeltaSeconds, InIterations);
	}
}

void UCharacterMovementComponentBase::MoveWithCollision(float InDeltaSeconds)
{
	const FVector PreviousLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = Velocity * InDeltaSeconds;
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit() && !bWaitingForTerrain)
	{
		HandleImpact(Hit, InDeltaSeconds, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}
	if (!bJustTeleported && InDeltaSeconds >= MIN_TICK_TIME && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - PreviousLocation) / InDeltaSeconds;
	}
}

UCharacterMovementComponentBase::EWaterExitStepResult UCharacterMovementComponentBase::TryWaterExitStep(
	const FVector& InRemainingDelta, const FHitResult& InHit)
{
	ICharacterMovementEnvironmentInterface* Provider = GetEnvironmentProvider();
	const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	const FCharacterMovementConstraint Constraint = GetSimulationConstraint();
	const FVector HorizontalDelta(InRemainingDelta.X, InRemainingDelta.Y, 0.0);
	const FVector HorizontalInput(Acceleration.X, Acceleration.Y, 0.0);
	if (!HasValidData() || !Provider || !Capsule || UpdatedPrimitive != Capsule ||
		!UpdatedPrimitive->IsQueryCollisionEnabled() || !CharacterOwner->GetActorEnableCollision() ||
		CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy || bWaitingForTerrain ||
		(MovementMode != MOVE_Swimming && !IsFloating()) || Constraint.bBlockMovement ||
		Constraint.Traversal != ECharacterTraversalMode::None || Environment.Result != ECharacterWorldQueryResult::Ready ||
		!Environment.bBodyInWater || Environment.bHeadUnderWater ||
		HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() ||
		!GetGravityDirection().Equals(FVector(0.0, 0.0, -1.0), KINDA_SMALL_NUMBER) ||
		!FMath::IsFinite(MaxStepHeight) || MaxStepHeight <= 0.f ||
		!FMath::IsFinite(PerchAdditionalHeight) ||
		!FMath::IsFinite(Capsule->GetScaledCapsuleHalfHeight()) || Capsule->GetScaledCapsuleHalfHeight() <= 0.f ||
		InRemainingDelta.ContainsNaN() || HorizontalInput.ContainsNaN() ||
		HorizontalDelta.IsNearlyZero() || HorizontalInput.IsNearlyZero() ||
		!InHit.IsValidBlockingHit() || InHit.bStartPenetrating || InHit.Time >= 1.f ||
		!InHit.GetComponent() || InHit.ImpactNormal.ContainsNaN() || FMath::Abs(InHit.ImpactNormal.Z) >= 0.35f ||
		FVector::DotProduct(HorizontalInput.GetSafeNormal(), HorizontalDelta.GetSafeNormal()) <= 0.f ||
		FVector::DotProduct(HorizontalInput.GetSafeNormal(), InHit.ImpactNormal) >= -KINDA_SMALL_NUMBER ||
		FVector::DotProduct(HorizontalDelta.GetSafeNormal(), InHit.ImpactNormal) >= -KINDA_SMALL_NUMBER ||
		!CanStepUp(InHit))
	{
		return EWaterExitStepResult::Rejected;
	}

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector SavedVelocity = Velocity;
	const FFindFloorResult SavedFloor = CurrentFloor;
	const bool bSavedJustTeleported = bJustTeleported;
	const float StepBudget = MaxStepHeight;
	const FVector Up(0.0, 0.0, StepBudget);


	FBox PathBounds = MakeMovementBounds(Start);
	PathBounds += MakeMovementBounds(Start + Up);
	PathBounds += MakeMovementBounds(Start + Up + HorizontalDelta);
	PathBounds += MakeMovementBounds(Start + HorizontalDelta);
	if (Provider->QueryMovementCoverage(PathBounds) != ECharacterWorldQueryResult::Ready)
	{
		bWaitingForTerrain = true;
		return EWaterExitStepResult::Waiting;
	}

	FScopedMovementUpdate ScopedMove(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	auto Reject = [&]()
	{
		const bool bWait = bWaitingForTerrain;
		ScopedMove.RevertMove();
		Velocity = SavedVelocity;
		CurrentFloor = SavedFloor;
		bJustTeleported = bSavedJustTeleported;

		bWaitingForTerrain = bWait;
		return bWait ? EWaterExitStepResult::Waiting : EWaterExitStepResult::Rejected;
	};
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();
	FHitResult UpHit;


	MoveUpdatedComponent(Up, Rotation, true, &UpHit);
	if (bWaitingForTerrain || UpHit.bBlockingHit || UpHit.bStartPenetrating ||
		!UpdatedComponent->GetComponentLocation().Equals(Start + Up, KINDA_SMALL_NUMBER))
	{
		return Reject();
	}
	FHitResult ForwardHit;
	MoveUpdatedComponent(HorizontalDelta, Rotation, true, &ForwardHit);
	if (bWaitingForTerrain || ForwardHit.bBlockingHit || ForwardHit.bStartPenetrating ||
		!UpdatedComponent->GetComponentLocation().Equals(Start + Up + HorizontalDelta, KINDA_SMALL_NUMBER))
	{
		return Reject();
	}
	FHitResult DownHit;
	MoveUpdatedComponent(-Up, Rotation, true, &DownHit);
	if (bWaitingForTerrain || !DownHit.IsValidBlockingHit() || DownHit.bStartPenetrating ||
		!DownHit.GetComponent() || !IsWalkable(DownHit) || !CanStepUp(DownHit))
	{
		return Reject();
	}
	const FVector CandidateLocation = UpdatedComponent->GetComponentLocation();
	const double Rise = CandidateLocation.Z - Start.Z;
	if (CandidateLocation.ContainsNaN() || Rise < 0.0 || Rise > StepBudget)
	{
		return Reject();
	}

	const double FloorReach = 2.0 * Capsule->GetScaledCapsuleHalfHeight() +
		2.0 * StepBudget + 2.0 * MAX_FLOOR_DIST + FMath::Max(0.f, PerchAdditionalHeight);
	if (!FMath::IsFinite(FloorReach))
	{
		return Reject();
	}
	FBox FloorBounds = MakeMovementBounds(CandidateLocation);
	FloorBounds += MakeMovementBounds(CandidateLocation - FVector(0.0, 0.0, FloorReach));
	if (Provider->QueryMovementCoverage(FloorBounds) != ECharacterWorldQueryResult::Ready)
	{
		bWaitingForTerrain = true;
		return Reject();
	}
	FFindFloorResult CandidateFloor;
	FindFloor(CandidateLocation, CandidateFloor, false, &DownHit);
	if (!CandidateFloor.IsWalkableFloor() || !CandidateFloor.HitResult.IsValidBlockingHit() ||
		CandidateFloor.HitResult.bStartPenetrating || !CanStepUp(CandidateFloor.HitResult) ||
		!FMath::IsFinite(CandidateFloor.GetDistanceToFloor()) ||
		CandidateFloor.GetDistanceToFloor() < 0.f || CandidateFloor.GetDistanceToFloor() > MAX_FLOOR_DIST)
	{
		return Reject();
	}
	const FCharacterMovementEnvironment CandidateEnvironment = Provider->SampleMovementEnvironment(CandidateLocation);
	if (CandidateEnvironment.Result != ECharacterWorldQueryResult::Ready ||
		CandidateEnvironment.WorldEpoch != Environment.WorldEpoch)
	{
		bWaitingForTerrain = true;
		return Reject();
	}
	if (CandidateEnvironment.bHeadUnderWater)
	{
		return Reject();
	}


	SetMovementEnvironment(CandidateEnvironment);
	return EWaterExitStepResult::Stepped;
}

void UCharacterMovementComponentBase::MoveWaterWithCollision(float InDeltaSeconds)
{
	const FVector PreviousLocation = UpdatedComponent->GetComponentLocation();
	const FVector PreviousVelocity = Velocity;
	FVector Delta = Velocity * InDeltaSeconds;
	float ImpactTime = InDeltaSeconds;
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	const FCharacterMovementConstraint Constraint = GetSimulationConstraint();
	const FVector HorizontalInput(Acceleration.X, Acceleration.Y, 0.0);
	if (Hit.IsValidBlockingHit() && Hit.GetComponent() && !bWaitingForTerrain && Delta.Z < 0.0 &&
		GetGravityDirection().Equals(FVector(0.0, 0.0, -1.0), KINDA_SMALL_NUMBER) &&
		Environment.bBodyInWater && !Environment.bHeadUnderWater &&
		!Constraint.bBlockMovement && Constraint.Traversal == ECharacterTraversalMode::None &&
		!HorizontalInput.IsNearlyZero() &&
		FVector::DotProduct(HorizontalInput, FVector(Delta.X, Delta.Y, 0.0)) > 0.0 &&
		Hit.ImpactNormal.Z > 1.f - KINDA_SMALL_NUMBER && IsWalkable(Hit) &&
		!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		const float RemainingFraction = 1.f - Hit.Time;
		HandleImpact(Hit, InDeltaSeconds, Delta);
		if (!HasValidData() || bWaitingForTerrain ||
			(MovementMode != MOVE_Swimming && !IsFloating()))
		{
			return;
		}
		Delta = FVector(Delta.X, Delta.Y, 0.0) * RemainingFraction;
		ImpactTime *= RemainingFraction;
		Hit = FHitResult();
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	}
	bool bStepped = false;
	if (Hit.IsValidBlockingHit() && !bWaitingForTerrain)
	{


		const EWaterExitStepResult Result = TryWaterExitStep(Delta * (1.f - Hit.Time), Hit);

		if (!HasValidData() || (MovementMode != MOVE_Swimming && !IsFloating()))
		{
			return;
		}
		if (Result == EWaterExitStepResult::Waiting)
		{
			Velocity = PreviousVelocity;
			return;
		}
		bStepped = Result == EWaterExitStepResult::Stepped;
		if (!bStepped)
		{
			HandleImpact(Hit, ImpactTime, Delta);
			SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		}
	}
	if (bWaitingForTerrain)
	{
		Velocity = PreviousVelocity;
		return;
	}
	if (!bJustTeleported && InDeltaSeconds >= MIN_TICK_TIME && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - PreviousLocation) / InDeltaSeconds;
	}
	if (bStepped)
	{


		Velocity.Z = 0.0;
		if (!Environment.bBodyInWater && !Environment.bHeadUnderWater)
		{
			SetMovementMode(MOVE_Falling);
		}
	}
}

void UCharacterMovementComponentBase::PhysClimbing(float InDeltaSeconds, int32 InIterations)
{
	if (InDeltaSeconds < MIN_TICK_TIME || !HasValidData())
	{
		return;
	}
	RefreshEnvironment();
	if (bWaitingForTerrain)
	{
		return;
	}
	if (!Environment.bClimbable || GetSimulationConstraint().Traversal != ECharacterTraversalMode::Climb)
	{
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(InDeltaSeconds, InIterations);
		return;
	}
	RestorePreAdditiveRootMotionVelocity();
	const FVector Normal = Environment.ClimbNormal.GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, -Normal).GetSafeNormal();
	const FVector Input = Acceleration / FMath::Max(1.f, MaxAcceleration);
	const double Vertical = FVector::DotProduct(Input, -Normal) + Input.Z;
	const FVector ClimbInput = (Right * FVector::DotProduct(Input, Right) + FVector::UpVector * Vertical).GetClampedToMaxSize(1.f);
	Acceleration = ClimbInput * GetMaxAcceleration();
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(InDeltaSeconds, GroundFriction, false, BrakingDecelerationWalking);
		Velocity = FVector::VectorPlaneProject(Velocity, Normal);
	}
	ApplyRootMotionToVelocity(InDeltaSeconds);
	MoveWithCollision(InDeltaSeconds);
}

void UCharacterMovementComponentBase::PhysWater(float InDeltaSeconds, int32 InIterations, bool bInFloat)
{
	if (InDeltaSeconds < MIN_TICK_TIME || !HasValidData())
	{
		return;
	}
	RefreshEnvironment();
	if (bWaitingForTerrain)
	{
		return;
	}
	if (!Environment.bBodyInWater && !Environment.bHeadUnderWater)
	{
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(InDeltaSeconds, InIterations);
		return;
	}
	RestorePreAdditiveRootMotionVelocity();
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(InDeltaSeconds, WaterFriction, true, BrakingDecelerationSwimming);
		if (bInFloat && FMath::Abs(Acceleration.Z) < KINDA_SMALL_NUMBER)
		{
			const double Error = Environment.FloatCenterZ - UpdatedComponent->GetComponentLocation().Z;
			Velocity.Z = FMath::Clamp(Error * FloatResponse, -static_cast<double>(MaxSwimSpeed), static_cast<double>(MaxSwimSpeed));
		}
	}
	ApplyRootMotionToVelocity(InDeltaSeconds);
	MoveWaterWithCollision(InDeltaSeconds);
}

FBox UCharacterMovementComponentBase::MakeMovementBounds(const FVector& InLocation) const
{
	const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return FBox(ForceInit);
	}
	const float Radius = Capsule->GetScaledCapsuleRadius() + 1.f;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() + 1.f;
	const FVector Extent(Radius, Radius, HalfHeight);
	return FBox(InLocation - Extent, InLocation + Extent);
}

bool UCharacterMovementComponentBase::MoveUpdatedComponentImpl(const FVector& InDelta, const FQuat& InRotation, bool bInSweep, FHitResult* OutHit, ETeleportType InTeleport)
{
	ICharacterMovementEnvironmentInterface* Provider = GetEnvironmentProvider();
	if (Provider && UpdatedComponent && CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy && !InDelta.IsNearlyZero())
	{
		const FVector Start = UpdatedComponent->GetComponentLocation();
		FBox Bounds = MakeMovementBounds(Start);
		Bounds += MakeMovementBounds(Start + InDelta);
		if (Provider->QueryMovementCoverage(Bounds) != ECharacterWorldQueryResult::Ready)
		{
			bWaitingForTerrain = true;
			if (OutHit)
			{
				*OutHit = FHitResult(0.f);
				OutHit->bBlockingHit = true;
				OutHit->Location = Start;
				OutHit->ImpactPoint = Start;
				OutHit->Normal = -InDelta.GetSafeNormal();
				OutHit->ImpactNormal = OutHit->Normal;
				OutHit->TraceStart = Start;
				OutHit->TraceEnd = Start + InDelta;
			}
			return false;
		}
	}
	return Super::MoveUpdatedComponentImpl(InDelta, InRotation, bInSweep, OutHit, InTeleport);
}

bool UCharacterMovementComponentBase::IsClimbing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(ECharacterCustomMovementMode::Climb);
}

bool UCharacterMovementComponentBase::IsFloating() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(ECharacterCustomMovementMode::Float);
}

bool UCharacterMovementComponentBase::IsSprinting() const
{
	return GetSimulationConstraint().bSprint && !GetSimulationConstraint().bBlockMovement;
}

bool UCharacterMovementComponentBase::IsWaitingForTerrain() const
{
	return bWaitingForTerrain;
}

bool UCharacterMovementComponentBase::IsHeadUnderWater() const
{
	return Environment.bHeadUnderWater;
}
