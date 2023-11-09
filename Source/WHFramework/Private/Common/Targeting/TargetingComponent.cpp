// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#include "Common/Targeting/TargetingComponent.h"

#include "Common/Targeting/TargetingAgentInterface.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Debug/DebugModuleTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

// Sets default values for this component's properties
UTargetingComponent::UTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	LockedOnWidgetClass = StaticLoadClass(UObject::StaticClass(), nullptr, TEXT("/Script/UMGEditor.WidgetBlueprint'/WHFramework/Common/Blueprints/Widget/WBP_LockOn.WBP_LockOn_C'"));
	TargetableActors = APawn::StaticClass();
	TargetableCollisionChannel = ECC_Pawn;
}

// Called when the game starts
void UTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerActor = GetOwner();
	if (!ensureEditorMsgf(OwnerActor, FString::Printf(TEXT("[%s] TargetingComponent: Cannot get Owner reference ..."), *GetName()), EDC_Ability, EDV_Error))
	{
		return;
	}

	OwnerPawn = Cast<APawn>(OwnerActor);
	if (!ensureEditorMsgf(OwnerPawn, FString::Printf(TEXT("[%s] TargetingComponent: Component is meant to be added to Pawn only ..."), *GetName()), EDC_Ability, EDV_Error))
	{
		return;
	}

	SetupLocalPlayerController();
}

void UTargetingComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bTargetLocked || !LockedOnTargetActor)
	{
		return;
	}

	if (!TargetIsTargetable(LockedOnTargetActor))
	{
		TargetLockOff();
		return;
	}

	SetControlRotationOnTarget(LockedOnTargetActor);

	// Target Locked Off based on Distance
	if (GetDistanceFromCharacter(LockedOnTargetActor) > MinimumDistanceToEnable)
	{
		TargetLockOff();
	}

	if (ShouldBreakLineOfSight() && !bIsBreakingLineOfSight)
	{
		if (BreakLineOfSightDelay <= 0)
		{
			TargetLockOff();
		}
		else
		{
			bIsBreakingLineOfSight = true;
			GetWorld()->GetTimerManager().SetTimer(
				LineOfSightBreakTimerHandle,
				this,
				&UTargetingComponent::BreakLineOfSight,
				BreakLineOfSightDelay
			);
		}
	}
}

void UTargetingComponent::TargetActor()
{
	ClosestTargetDistance = MinimumDistanceToEnable;

	if (bTargetLocked)
	{
		TargetLockOff();
	}
	else
	{
		const TArray<AActor*> Actors = GetAllActorsOfClass(TargetableActors);
		LockedOnTargetActor = FindNearestTarget(Actors);
		TargetLockOn(LockedOnTargetActor);
	}
}

void UTargetingComponent::TargetActorWithAxisInput(const float AxisValue)
{
	// If we're not locked on, do nothing
	if (!bTargetLocked)
	{
		return;
	}

	if (!LockedOnTargetActor)
	{
		return;
	}

	// If we're not allowed to switch target, do nothing
	if (!ShouldSwitchTargetActor(AxisValue))
	{
		return;
	}

	// If we're switching target, do nothing for a set amount of time
	if (bIsSwitchingTarget)
	{
		return;
	}

	// Lock off target
	AActor* CurrentTarget = LockedOnTargetActor;

	// Depending on Axis Value negative / positive, set Direction to Look for (negative: left, positive: right)
	const float RangeMin = AxisValue < 0 ? 0 : 180;
	const float RangeMax = AxisValue < 0 ? 180 : 360;

	// Reset Closest Target Distance to Minimum Distance to Enable
	ClosestTargetDistance = MinimumDistanceToEnable;

	// Get All Actors of Class
	TArray<AActor*> Actors = GetAllActorsOfClass(TargetableActors);

	// For each of these actors, check line trace and ignore Current Target and build the list of actors to look from
	TArray<AActor*> ActorsToLook;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(CurrentTarget);
	for (AActor* Actor : Actors)
	{
		const bool bHit = LineTraceForActor(Actor, ActorsToIgnore);
		if (bHit && IsInViewport(Actor))
		{
			ActorsToLook.Add(Actor);
		}
	}

	// Find Targets in Range (left or right, based on Character and CurrentTarget)
	TArray<AActor*> TargetsInRange = FindTargetsInRange(ActorsToLook, RangeMin, RangeMax);

	// For each of these targets in range, get the closest one to current target
	AActor* ActorToTarget = nullptr;
	for (AActor* Actor : TargetsInRange)
	{
		// and filter out any character too distant from minimum distance to enable
		const float Distance = GetDistanceFromCharacter(Actor);
		if (Distance < MinimumDistanceToEnable)
		{
			const float RelativeActorsDistance = CurrentTarget->GetDistanceTo(Actor);
			if (RelativeActorsDistance < ClosestTargetDistance)
			{
				ClosestTargetDistance = RelativeActorsDistance;
				ActorToTarget = Actor;
			}
		}
	}

	if (ActorToTarget)
	{
		if (SwitchingTargetTimerHandle.IsValid())
		{
			SwitchingTargetTimerHandle.Invalidate();
		}

		TargetLockOff();
		LockedOnTargetActor = ActorToTarget;
		TargetLockOn(ActorToTarget);

		GetWorld()->GetTimerManager().SetTimer(
			SwitchingTargetTimerHandle,
			this,
			&UTargetingComponent::ResetIsSwitchingTarget,
			// Less sticky if still switching
			bIsSwitchingTarget ? 0.25f : 0.5f
		);

		bIsSwitchingTarget = true;
	}
}

bool UTargetingComponent::GetTargetLockedStatus()
{
	return bTargetLocked;
}

AActor* UTargetingComponent::GetLockedOnTargetActor() const
{
	return LockedOnTargetActor;
}

bool UTargetingComponent::IsLocked() const
{
	return bTargetLocked && LockedOnTargetActor;
}

TArray<AActor*> UTargetingComponent::FindTargetsInRange(TArray<AActor*> ActorsToLook, const float RangeMin, const float RangeMax) const
{
	TArray<AActor*> ActorsInRange;

	for (AActor* Actor : ActorsToLook)
	{
		const float Angle = GetAngleUsingCameraRotation(Actor);
		if (Angle > RangeMin && Angle < RangeMax)
		{
			ActorsInRange.Add(Actor);
		}
	}

	return ActorsInRange;
}

float UTargetingComponent::GetAngleUsingCameraRotation(const AActor* ActorToLook) const
{
	UCameraComponent* CameraComponent = OwnerActor->FindComponentByClass<UCameraComponent>();
	if (!CameraComponent)
	{
		// Fallback to CharacterRotation if no CameraComponent can be found
		return GetAngleUsingCharacterRotation(ActorToLook);
	}

	const FRotator CameraWorldRotation = CameraComponent->GetComponentRotation();
	const FRotator LookAtRotation = FindLookAtRotation(CameraComponent->GetComponentLocation(), ActorToLook->GetActorLocation());

	float YawAngle = CameraWorldRotation.Yaw - LookAtRotation.Yaw;
	if (YawAngle < 0)
	{
		YawAngle = YawAngle + 360;
	}

	return YawAngle;
}

float UTargetingComponent::GetAngleUsingCharacterRotation(const AActor* ActorToLook) const
{
	const FRotator CharacterRotation = OwnerActor->GetActorRotation();
	const FRotator LookAtRotation = FindLookAtRotation(OwnerActor->GetActorLocation(), ActorToLook->GetActorLocation());

	float YawAngle = CharacterRotation.Yaw - LookAtRotation.Yaw;
	if (YawAngle < 0)
	{
		YawAngle = YawAngle + 360;
	}

	return YawAngle;
}

FRotator UTargetingComponent::FindLookAtRotation(const FVector Start, const FVector Target)
{
	return FRotationMatrix::MakeFromX(Target - Start).Rotator();
}

void UTargetingComponent::ResetIsSwitchingTarget()
{
	bIsSwitchingTarget = false;
	bDesireToSwitch = false;
}

bool UTargetingComponent::ShouldSwitchTargetActor(const float AxisValue)
{
	// Sticky feeling computation
	if (bEnableStickyTarget)
	{
		StartRotatingStack += (AxisValue != 0) ? AxisValue * AxisMultiplier : (StartRotatingStack > 0 ? -AxisMultiplier : AxisMultiplier);

		if (AxisValue == 0 && FMath::Abs(StartRotatingStack) <= AxisMultiplier)
		{
			StartRotatingStack = 0.0f;
		}

		// If Axis value does not exceeds configured threshold, do nothing
		if (FMath::Abs(StartRotatingStack) < StickyRotationThreshold)
		{
			bDesireToSwitch = false;
			return false;
		}

		//Sticky when switching target.
		if (StartRotatingStack * AxisValue > 0)
		{
			StartRotatingStack = StartRotatingStack > 0 ? StickyRotationThreshold : -StickyRotationThreshold;
		}
		else if (StartRotatingStack * AxisValue < 0)
		{
			StartRotatingStack = StartRotatingStack * -1.0f;
		}

		bDesireToSwitch = true;

		return true;
	}

	// Non Sticky feeling, check Axis value exceeds threshold
	return FMath::Abs(AxisValue) > StartRotatingThreshold;
}

void UTargetingComponent::TargetLockOn(AActor* TargetToLockOn)
{
	if (!IsValid(TargetToLockOn))
	{
		return;
	}

	// Recast PlayerController in case it wasn't already setup on Begin Play (local split screen)
	SetupLocalPlayerController();

	bTargetLocked = true;
	if (bShouldDrawLockedOnWidget)
	{
		CreateAndAttachTargetLockedOnWidgetComponent(TargetToLockOn);
	}

	if (bShouldControlRotation)
	{
		ControlRotation(true);
	}

	if (bAdjustPitchBasedOnDistanceToTarget || bIgnoreLookInput)
	{
		if (IsValid(OwnerPlayerController))
		{
			OwnerPlayerController->SetIgnoreLookInput(true);
		}
	}

	if (OnTargetLockedOn.IsBound())
	{
		OnTargetLockedOn.Broadcast(TargetToLockOn);
	}
}

void UTargetingComponent::TargetLockOff()
{
	// Recast PlayerController in case it wasn't already setup on Begin Play (local split screen)
	SetupLocalPlayerController();

	bTargetLocked = false;
	if (TargetLockedOnWidgetComponent)
	{
		TargetLockedOnWidgetComponent->DestroyComponent();
	}

	if (LockedOnTargetActor)
	{
		if (bShouldControlRotation)
		{
			ControlRotation(false);
		}

		if (IsValid(OwnerPlayerController))
		{
			OwnerPlayerController->ResetIgnoreLookInput();
		}

		if (OnTargetLockedOff.IsBound())
		{
			OnTargetLockedOff.Broadcast(LockedOnTargetActor);
		}
	}

	LockedOnTargetActor = nullptr;
}

void UTargetingComponent::SetShouldControlRotation(bool bValue)
{
	ControlRotation(bShouldControlRotation = bValue);
}

void UTargetingComponent::CreateAndAttachTargetLockedOnWidgetComponent(AActor* TargetActor)
{
	if (!LockedOnWidgetClass)
	{
		WHLog(TEXT("TargetingComponent: Cannot get LockedOnWidgetClass, please ensure it is a valid reference in the Component Properties."), EDC_Ability, EDV_Error);
		return;
	}

	TargetLockedOnWidgetComponent = NewObject<UWidgetComponent>(TargetActor, MakeUniqueObjectName(TargetActor, UWidgetComponent::StaticClass(), FName("TargetLockOn")));
	TargetLockedOnWidgetComponent->SetWidgetClass(LockedOnWidgetClass);

	UMeshComponent* MeshComponent = TargetActor->FindComponentByClass<UMeshComponent>();
	USceneComponent* ParentComponent = MeshComponent && LockedOnWidgetParentSocket != NAME_None ? MeshComponent : TargetActor->GetRootComponent();

	if (IsValid(OwnerPlayerController))
	{
		TargetLockedOnWidgetComponent->SetOwnerPlayer(OwnerPlayerController->GetLocalPlayer());
	}

	TargetLockedOnWidgetComponent->ComponentTags.Add(FName("Targeting.LockOnWidget"));
	TargetLockedOnWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	TargetLockedOnWidgetComponent->SetupAttachment(ParentComponent, LockedOnWidgetParentSocket);
	TargetLockedOnWidgetComponent->SetRelativeLocation(LockedOnWidgetRelativeLocation);
	TargetLockedOnWidgetComponent->SetDrawSize(FVector2D(LockedOnWidgetDrawSize, LockedOnWidgetDrawSize));
	TargetLockedOnWidgetComponent->SetVisibility(true);
	TargetLockedOnWidgetComponent->RegisterComponent();
}

TArray<AActor*> UTargetingComponent::GetAllActorsOfClass(const TSubclassOf<AActor> ActorClass) const
{
	TArray<AActor*> Actors;
	for (TActorIterator<AActor> ActorIterator(GetWorld(), ActorClass); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		const bool bIsTargetable = TargetIsTargetable(Actor);
		if (bIsTargetable)
		{
			Actors.Add(Actor);
		}
	}

	return Actors;
}

bool UTargetingComponent::TargetIsTargetable(const AActor* Actor)
{
	const bool bIsImplemented = Actor->GetClass()->ImplementsInterface(UTargetingAgentInterface::StaticClass());
	if (bIsImplemented)
	{
		return ITargetingAgentInterface::Execute_IsTargetable(Actor);
	}

	return true;
}

void UTargetingComponent::SetupLocalPlayerController()
{
	if (!IsValid(OwnerPawn))
	{
		WHLog(FString::Printf(TEXT("[%s] TargetingComponent: Component is meant to be added to Pawn only ..."), *GetName()), EDC_Ability, EDV_Error);
		return;
	}

	OwnerPlayerController = Cast<APlayerController>(OwnerPawn->GetController());
}

AActor* UTargetingComponent::FindNearestTarget(TArray<AActor*> Actors) const
{
	TArray<AActor*> ActorsHit;

	// Find all actors we can line trace to
	for (AActor* Actor : Actors)
	{
		const bool bHit = LineTraceForActor(Actor);
		if (bHit && IsInViewport(Actor))
		{
			ActorsHit.Add(Actor);
		}
	}

	// From the hit actors, check distance and return the nearest
	if (ActorsHit.Num() == 0)
	{
		return nullptr;
	}

	float ClosestDistance = ClosestTargetDistance;
	AActor* Target = nullptr;
	for (AActor* Actor : ActorsHit)
	{
		const float Distance = GetDistanceFromCharacter(Actor);
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			Target = Actor;
		}
	}

	return Target;
}


bool UTargetingComponent::LineTraceForActor(AActor* OtherActor, const TArray<AActor*> ActorsToIgnore) const
{
	FHitResult HitResult;
	const bool bHit = LineTrace(HitResult, OtherActor, ActorsToIgnore);
	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor == OtherActor)
		{
			return true;
		}
	}

	return false;
}

bool UTargetingComponent::LineTrace(FHitResult& HitResult, const AActor* OtherActor, const TArray<AActor*> ActorsToIgnore) const
{
	FCollisionQueryParams Params = FCollisionQueryParams(FName("LineTraceSingle"));

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Init(OwnerActor, 1);
	IgnoredActors += ActorsToIgnore;

	Params.AddIgnoredActors(IgnoredActors);

	return GetWorld()->LineTraceSingleByChannel(
		HitResult,
		OwnerActor->GetActorLocation(),
		OtherActor->GetActorLocation(),
		TargetableCollisionChannel,
		Params
	);
}

FRotator UTargetingComponent::GetControlRotationOnTarget(const AActor* OtherActor) const
{
	if (!IsValid(OwnerPlayerController))
	{
		WHLog(TEXT("UTargetingComponent::GetControlRotationOnTarget - OwnerPlayerController is not valid ..."), EDC_Ability, EDV_Error);
		return FRotator::ZeroRotator;
	}

	const FRotator ControlRotation = OwnerPlayerController->GetControlRotation();

	const FVector CharacterLocation = OwnerActor->GetActorLocation();
	const FVector OtherActorLocation = OtherActor->GetActorLocation();

	// Find look at rotation
	const FRotator LookRotation = FRotationMatrix::MakeFromX(OtherActorLocation - CharacterLocation).Rotator();
	float Pitch = LookRotation.Pitch;
	FRotator TargetRotation;
	if (bAdjustPitchBasedOnDistanceToTarget)
	{
		const float DistanceToTarget = GetDistanceFromCharacter(OtherActor);
		const float PitchInRange = (DistanceToTarget * PitchDistanceCoefficient + PitchDistanceOffset) * -1.0f;
		const float PitchOffset = FMath::Clamp(PitchInRange, PitchMin, PitchMax);

		Pitch = Pitch + PitchOffset;
		TargetRotation = FRotator(Pitch, LookRotation.Yaw, ControlRotation.Roll);
	}
	else
	{
		if (bIgnoreLookInput)
		{
			TargetRotation = FRotator(Pitch, LookRotation.Yaw, ControlRotation.Roll);
		}
		else
		{
			TargetRotation = FRotator(ControlRotation.Pitch, LookRotation.Yaw, ControlRotation.Roll);
		}
	}

	return FMath::RInterpTo(ControlRotation, TargetRotation, GetWorld()->GetDeltaSeconds(), 9.0f);
}

void UTargetingComponent::SetControlRotationOnTarget(AActor* TargetActor) const
{
	if (!IsValid(OwnerPlayerController))
	{
		return;
	}

	const FRotator ControlRotation = GetControlRotationOnTarget(TargetActor);
	if (OnTargetSetRotation.IsBound())
	{
		OnTargetSetRotation.Broadcast(TargetActor, ControlRotation);
	}
	else
	{
		OwnerPlayerController->SetControlRotation(ControlRotation);
	}
}

float UTargetingComponent::GetDistanceFromCharacter(const AActor* OtherActor) const
{
	return OwnerActor->GetDistanceTo(OtherActor);
}

bool UTargetingComponent::ShouldBreakLineOfSight() const
{
	if (!LockedOnTargetActor)
	{
		return true;
	}

	TArray<AActor*> ActorsToIgnore = GetAllActorsOfClass(TargetableActors);
	ActorsToIgnore.Remove(LockedOnTargetActor);

	FHitResult HitResult;
	const bool bHit = LineTrace(HitResult, LockedOnTargetActor, ActorsToIgnore);
	if (bHit && HitResult.GetActor() != LockedOnTargetActor)
	{
		return true;
	}

	return false;
}

void UTargetingComponent::BreakLineOfSight()
{
	bIsBreakingLineOfSight = false;
	if (ShouldBreakLineOfSight())
	{
		TargetLockOff();
	}
}

void UTargetingComponent::ControlRotation(const bool ShouldControlRotation) const
{
	if (!IsValid(OwnerPawn))
	{
		return;
	}

	OwnerPawn->bUseControllerRotationYaw = ShouldControlRotation;

	UCharacterMovementComponent* CharacterMovementComponent = OwnerPawn->FindComponentByClass<UCharacterMovementComponent>();
	if (CharacterMovementComponent)
	{
		CharacterMovementComponent->bOrientRotationToMovement = !ShouldControlRotation;
	}
}

bool UTargetingComponent::IsInViewport(const AActor* TargetActor) const
{
	if (!IsValid(OwnerPlayerController))
	{
		return true;
	}

	FVector2D ScreenLocation;
	OwnerPlayerController->ProjectWorldLocationToScreen(TargetActor->GetActorLocation(), ScreenLocation);

	FVector2D ViewportSize;
	GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);

	return ScreenLocation.X > 0 && ScreenLocation.Y > 0 && ScreenLocation.X < ViewportSize.X && ScreenLocation.Y < ViewportSize.Y;
}
