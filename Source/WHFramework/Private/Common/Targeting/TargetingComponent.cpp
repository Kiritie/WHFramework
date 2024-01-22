// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#include "Common/Targeting/TargetingComponent.h"

#include "Common/Targeting/TargetingAgentInterface.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Debug/DebugModuleTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Widget/WidgetModuleStatics.h"

// Sets default values for this component's properties
UTargetingComponent::UTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	LockedOnWidgetClass = StaticLoadClass(UObject::StaticClass(), nullptr, TEXT("/Script/UMGEditor.WidgetBlueprint'/WHFramework/Common/Blueprints/Widget/WBP_LockOnBase.WBP_LockOnBase_C'"));
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

	// 根据距离锁定目标
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
	// 如果我们没有被锁定，什么都不要做
	if (!bTargetLocked)
	{
		return;
	}

	if (!LockedOnTargetActor)
	{
		return;
	}

	// 如果不允许我们切换目标，什么都不要做
	if (!ShouldSwitchTargetActor(AxisValue))
	{
		return;
	}

	// 如果我们要转换目标，在一段时间内什么都不做
	if (bIsSwitchingTarget)
	{
		return;
	}

	// 锁定目标
	AActor* CurrentTarget = LockedOnTargetActor;

	// 根据轴值的负/正，设置寻找方向(负:左，正:右)
	const float RangeMin = AxisValue < 0 ? 0 : 180;
	const float RangeMax = AxisValue < 0 ? 180 : 360;

	// 将最近目标距离重置为最小距离为Enable
	ClosestTargetDistance = MinimumDistanceToEnable;

	// 找到所有的Actor
	TArray<AActor*> Actors = GetAllActorsOfClass(TargetableActors);

	// 遍历所有的Actor，选中行跟踪并忽略Current Target，并构建要查看的参与者列表
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

	// 在范围内查找目标(根据角色和当前目标向左或向右)
	TArray<AActor*> TargetsInRange = FindTargetsInRange(ActorsToLook, RangeMin, RangeMax);

	// 对于这些范围内的每一个目标，取离当前目标最近的一个
	AActor* ActorToTarget = nullptr;
	for (AActor* Actor : TargetsInRange)
	{
		// 过滤掉任何离最小距离太远而无法启用的字符
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
			// 如果仍在切换，粘性会降低
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
		// 如果找不到cameracomcomponent，则退回到CharacterRotation
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
	// 粘性计算
	if (bEnableStickyTarget)
	{
		StartRotatingStack += (AxisValue != 0) ? AxisValue * AxisMultiplier : (StartRotatingStack > 0 ? -AxisMultiplier : AxisMultiplier);

		if (AxisValue == 0 && FMath::Abs(StartRotatingStack) <= AxisMultiplier)
		{
			StartRotatingStack = 0.0f;
		}

		// 如果Axis值不超过配置的阈值，则不执行任何操作
		if (FMath::Abs(StartRotatingStack) < StickyRotationThreshold)
		{
			bDesireToSwitch = false;
			return false;
		}

		// 切换目标时粘滞
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

	// 无粘滞感，检查轴值超过阈值
	return FMath::Abs(AxisValue) > StartRotatingThreshold;
}

void UTargetingComponent::TargetLockOn(AActor* TargetToLockOn)
{
	if (!IsValid(TargetToLockOn))
	{
		return;
	}

	// 重铸PlayerController，以防它还没有设置在开始播放(本地分屏)
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
	// 重铸PlayerController，以防它还没有设置在开始播放(本地分屏)
	SetupLocalPlayerController();

	bTargetLocked = false;
	if (TargetLockedOnWidget)
	{
		TargetLockedOnWidget->Destroy(true);
		TargetLockedOnWidget = nullptr;
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

	UMeshComponent* MeshComponent = TargetActor->FindComponentByClass<UMeshComponent>();
	USceneComponent* ParentComponent = MeshComponent && LockedOnWidgetParentSocket != NAME_None ? MeshComponent : TargetActor->GetRootComponent();

	TargetLockedOnWidget = UWidgetModuleStatics::CreateWorldWidget(LockedOnWidgetClass, TargetActor, FWorldWidgetMapping(ParentComponent, LockedOnWidgetParentSocket, LockedOnWidgetRelativeLocation), {});
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

	// 找到所有能追踪到的演员
	for (AActor* Actor : Actors)
	{
		const bool bHit = LineTraceForActor(Actor);
		if (bHit && IsInViewport(Actor))
		{
			ActorsHit.Add(Actor);
		}
	}

	// 从命中的演员，检查距离并返回最近的
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
