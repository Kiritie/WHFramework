#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Actor/CameraRigBase.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraModule.h"
#include "Camera/Components/CameraSpringArmComponent.h"
#include "Camera/Director/CameraDirectorBase.h"
#include "Camera/Feature/CameraFeatureBase.h"
#include "Camera/Interface/CameraTrackableInterface.h"
#include "Camera/Mode/CameraModeBase.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Math/MathHelper.h"

ACameraManagerBase::ACameraManagerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ACameraManagerBase::InitializeFor(APlayerController* PC)
{
	Super::InitializeFor(PC);
	if(!PC || !PC->IsLocalController() || !GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	LocalPlayerIndex = 0;
	PC->IsSplitscreenPlayer(&LocalPlayerIndex);

	SpawnRuntimeRig();
	UCameraModule::Get().RegisterCameraManager(this);

	const FCameraConfig& CameraConfig = UCameraModule::Get().GetCameraConfig();
	DesiredState.PivotRotation = CameraConfig.DefaultRotationOffset;
	DesiredState.ArmLength = CameraConfig.DefaultDistance;
	DesiredState.SocketOffset = CameraConfig.DefaultSocketOffset;
	DesiredState.FOV = CameraConfig.DefaultFOV;
	DesiredState.bEnableCollision = CameraConfig.bEnableCollision;
	CurrentArmLength = DesiredState.ArmLength;

	if(RuntimeRig)
	{
		PC->SetViewTarget(RuntimeRig);
	}

	ActivateDefaultMode();
}

void ACameraManagerBase::Destroyed()
{
	ACameraRigBase* DefaultRig = nullptr;
	if(UCameraModule::IsValid())
	{
		DefaultRig = UCameraModule::Get().GetDefaultRig();
		UCameraModule::Get().UnRegisterCameraManager(this);
	}

	StopDirector();
	for(UCameraFeatureBase* Feature : ActiveFeatures)
	{
		if(Feature)
		{
			Feature->Exit();
		}
	}

	if(RuntimeRig && RuntimeRig != DefaultRig)
	{
		RuntimeRig->Destroy();
	}

	Super::Destroyed();
}

void ACameraManagerBase::SpawnRuntimeRig()
{
	if(RuntimeRig || !GetWorld())
	{
		return;
	}

	if(LocalPlayerIndex == 0 && IsValid(UCameraModule::Get().GetDefaultRig()))
	{
		RuntimeRig = UCameraModule::Get().GetDefaultRig();
		RuntimeRig->SetOwningCameraManager(this);
		return;
	}

	TSubclassOf<ACameraRigBase> Class = UCameraModule::Get().GetDefaultRigClass();
	if(!Class)
	{
		Class = ACameraRigBase::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.Owner = PCOwner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	RuntimeRig = GetWorld()->SpawnActor<ACameraRigBase>(Class, FTransform::Identity, Params);
	if(RuntimeRig)
	{
		RuntimeRig->SetOwningCameraManager(this);
	}
}

void ACameraManagerBase::SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams Params)
{
	Super::SetViewTarget(RuntimeRig ? RuntimeRig.Get() : NewViewTarget, Params);
}

void ACameraManagerBase::ActivateDefaultMode()
{
	FCameraModeContext Context;
	Context.Transition = FCameraTransitionParams::Instant();
	SetMode(UCameraModule::Get().GetDefaultModeClass(), Context);
}

void ACameraManagerBase::SetMode(TSubclassOf<UCameraModeBase> Class, const FCameraModeContext& Context)
{
	if(!Class)
	{
		return;
	}

	TransitionSource = DesiredState;
	ActiveTransition = Context.Transition;
	TransitionElapsed = 0.f;
	bTransitionActive = !ActiveTransition.bInstant && ActiveTransition.Duration > 0.f;
	if(ActiveMode)
	{
		ActiveMode->Exit();
	}

	bResolvedCameraConfigDirty = true;
	ActiveMode = NewObject<UCameraModeBase>(this, Class);
	ActiveMode->Enter(this, Context);
}

void ACameraManagerBase::SetDefaultMode(const FCameraTransitionParams& Transition)
{
	FCameraModeContext Context;
	Context.Transition = Transition;
	SetMode(UCameraModule::Get().GetDefaultModeClass(), Context);
}

void ACameraManagerBase::ClearModeTarget()
{
	SetDefaultMode(FCameraTransitionParams::Smooth());
}

void ACameraManagerBase::SetInitialView(const FRotator& Rotation, float Distance)
{
	DesiredState.PivotRotation = Rotation;
	DesiredState.ArmLength = FMath::Max(0.f, Distance);
	bTransitionActive = false;
}

void ACameraManagerBase::SetDesiredPivot(const FVector& Location, const FRotator& Rotation)
{
	DesiredState.PivotLocation = Location;
	DesiredState.PivotRotation = Rotation;
	bTransitionActive = false;
}

void ACameraManagerBase::AddInputIntent(const FCameraInputIntent& Input)
{
	if(!ActiveMode)
	{
		return;
	}

	ActiveMode->AddInput(ResolveInput(Input));
}

FCameraInputIntent ACameraManagerBase::ResolveInput(const FCameraInputIntent& InRawInput) const
{
	const FCameraConfig& Config = UCameraModule::Get().GetCameraConfig();
	FCameraInputIntent Result = InRawInput;
	Result.Move *= Config.Movement.MoveSpeed;
	Result.Look.X *= Config.Rotation.HorizontalSpeed * UserSettings.HorizontalLookSensitivity;
	Result.Look.Y *= Config.Rotation.VerticalSpeed * UserSettings.VerticalLookSensitivity;
	Result.Pan.X *= Config.Movement.HorizontalPanSpeed * UserSettings.HorizontalPanSensitivity;
	Result.Pan.Y *= Config.Movement.VerticalPanSpeed * UserSettings.VerticalPanSensitivity;
	Result.Zoom *= Config.Zoom.Speed * UserSettings.ZoomSensitivity;

	if(UserSettings.bInvertHorizontalLook)
	{
		Result.Look.X *= -1.f;
	}
	if(UserSettings.bInvertVerticalLook)
	{
		Result.Look.Y *= -1.f;
	}
	if(UserSettings.bInvertHorizontalPan)
	{
		Result.Pan.X *= -1.f;
	}
	if(UserSettings.bInvertVerticalPan)
	{
		Result.Pan.Y *= -1.f;
	}
	if(!Config.Movement.bEnableVerticalPan)
	{
		Result.Pan.Y = 0.f;
	}
	if(Result.bBoost)
	{
		Result.Move *= Config.Movement.BoostMultiplier;
	}

	return Result;
}

FCameraConfigOverrideHandle ACameraManagerBase::PushCameraConfigOverride(const FCameraConfigOverride& InOverride, int32 InPriority)
{
	FCameraConfigOverrideHandle Handle;
	Handle.ID = FGuid::NewGuid();

	FCameraConfigOverrideEntry Entry;
	Entry.Handle = Handle;
	Entry.Priority = InPriority;
	Entry.Sequence = ++CameraOverrideSequence;
	Entry.Data = InOverride;
	CameraConfigOverrides.Add(Entry);
	CameraConfigOverrides.Sort([](const FCameraConfigOverrideEntry& A, const FCameraConfigOverrideEntry& B)
	{
		return A.Priority == B.Priority ? A.Sequence < B.Sequence : A.Priority < B.Priority;
	});
	bResolvedCameraConfigDirty = true;
	return Handle;
}

void ACameraManagerBase::PopCameraConfigOverride(FCameraConfigOverrideHandle InHandle)
{
	if(!InHandle.IsValid())
	{
		return;
	}

	const int32 Removed = CameraConfigOverrides.RemoveAll([&InHandle](const FCameraConfigOverrideEntry& Entry)
	{
		return Entry.Handle.ID == InHandle.ID;
	});
	if(Removed > 0)
	{
		bResolvedCameraConfigDirty = true;
	}
}

void ACameraManagerBase::ClearCameraConfigOverrides()
{
	if(!CameraConfigOverrides.IsEmpty())
	{
		CameraConfigOverrides.Reset();
		bResolvedCameraConfigDirty = true;
	}
}

const FCameraResolvedConfig& ACameraManagerBase::GetResolvedCameraConfig()
{
	if(bResolvedCameraConfigDirty)
	{
		CachedResolvedCameraConfig = ResolveCameraConfig(GetActiveTargetParams());
		bResolvedCameraConfigDirty = false;
	}
	return CachedResolvedCameraConfig;
}

const FCameraTargetParams* ACameraManagerBase::GetActiveTargetParams() const
{
	return ActiveMode ? ActiveMode->GetTargetParams() : nullptr;
}

FCameraResolvedConfig ACameraManagerBase::ResolveCameraConfig(const FCameraTargetParams* InTargetParams) const
{
	const FCameraConfig& Config = UCameraModule::Get().GetCameraConfig();
	FCameraResolvedConfig Result;
	Result.PivotOffset = Config.DefaultPivotOffset;
	Result.SocketOffset = Config.DefaultSocketOffset;
	Result.RotationOffset = Config.DefaultRotationOffset;
	Result.Distance = Config.DefaultDistance;
	Result.DistanceRange = Config.Zoom.DistanceRange;
	Result.PitchRange = Config.Rotation.PitchRange;
	Result.FOV = Config.DefaultFOV;
	Result.bEnableCollision = Config.bEnableCollision;

	if(InTargetParams)
	{
		Result.PivotOffset = InTargetParams->PivotOffset;
		Result.SocketOffset = InTargetParams->SocketOffset;
		Result.RotationOffset += InTargetParams->RotationOffset;
		Result.Distance = InTargetParams->Distance;
		if(InTargetParams->bOverrideDistanceRange)
		{
			Result.DistanceRange = InTargetParams->DistanceRange;
		}
		if(InTargetParams->bOverridePitchRange)
		{
			Result.PitchRange = InTargetParams->PitchRange;
		}
	}

	for(const FCameraConfigOverrideEntry& Entry : CameraConfigOverrides)
	{
		const FCameraConfigOverride& Override = Entry.Data;
		if(Override.bOverridePivotOffset)
		{
			Result.PivotOffset = Override.PivotOffset;
		}
		if(Override.bOverrideSocketOffset)
		{
			Result.SocketOffset = Override.SocketOffset;
		}
		if(Override.bOverrideRotationOffset)
		{
			Result.RotationOffset = Override.RotationOffset;
		}
		if(Override.bOverrideDistance)
		{
			Result.Distance = Override.Distance;
		}
		if(Override.bOverrideDistanceRange)
		{
			Result.DistanceRange = Override.DistanceRange;
		}
		if(Override.bOverridePitchRange)
		{
			Result.PitchRange = Override.PitchRange;
		}
		if(Override.bOverrideFOV)
		{
			Result.FOV = Override.FOV;
		}
	}
	return Result;
}

FCameraFeatureHandle ACameraManagerBase::PushFeature(TSubclassOf<UCameraFeatureBase> Class, const FCameraFeatureContext& Context)
{
	FCameraFeatureHandle Handle;
	if(!Class)
	{
		return Handle;
	}

	Handle.Id = FGuid::NewGuid();
	UCameraFeatureBase* Feature = NewObject<UCameraFeatureBase>(this, Class);
	Feature->SetHandle(Handle);
	Feature->Enter(this, Context);
	ActiveFeatures.Add(Feature);
	ActiveFeatures.Sort([](const UCameraFeatureBase& A, const UCameraFeatureBase& B)
	{
		return A.GetPriority() < B.GetPriority();
	});
	TransitionSource = DesiredState;
	ActiveTransition = Context.EnterTransition;
	TransitionElapsed = 0.f;
	bTransitionActive = !ActiveTransition.bInstant && ActiveTransition.Duration > 0.f;
	return Handle;
}

void ACameraManagerBase::PopFeature(FCameraFeatureHandle Handle)
{
	const int32 Index = ActiveFeatures.IndexOfByPredicate([&](const UCameraFeatureBase* Feature)
	{
		return Feature && Feature->GetHandle() == Handle;
	});
	if(Index != INDEX_NONE)
	{
		TransitionSource = DesiredState;
		ActiveTransition = ActiveFeatures[Index]->GetContext().ExitTransition;
		TransitionElapsed = 0.f;
		bTransitionActive = !ActiveTransition.bInstant && ActiveTransition.Duration > 0.f;
		ActiveFeatures[Index]->Exit();
		ActiveFeatures.RemoveAt(Index);
	}
}

void ACameraManagerBase::StartDirector(TSubclassOf<UCameraDirectorBase> Class)
{
	StopDirector();
	if(Class)
	{
		ActiveDirector = NewObject<UCameraDirectorBase>(this, Class);
		ActiveDirector->Start(this);
	}
}

void ACameraManagerBase::StopDirector()
{
	if(ActiveDirector)
	{
		ActiveDirector->Stop();
	}
	ActiveDirector = nullptr;
}

void ACameraManagerBase::ApplyRig(float DeltaTime)
{
	if(!RuntimeRig)
	{
		return;
	}

	FCameraRigState State = DesiredState;
	if(bTransitionActive)
	{
		TransitionElapsed = FMath::Min(TransitionElapsed + DeltaTime, ActiveTransition.Duration);
		const float Alpha = FMathHelper::EvaluateByEaseType(ActiveTransition.EaseType, TransitionElapsed, ActiveTransition.Duration);
		State.PivotLocation = FMath::Lerp(TransitionSource.PivotLocation, DesiredState.PivotLocation, Alpha);
		State.PivotRotation = FMathHelper::LerpRotator(TransitionSource.PivotRotation, DesiredState.PivotRotation, Alpha, true);
		State.ArmLength = FMath::Lerp(TransitionSource.ArmLength, DesiredState.ArmLength, Alpha);
		State.SocketOffset = FMath::Lerp(TransitionSource.SocketOffset, DesiredState.SocketOffset, Alpha);
		State.FOV = FMath::Lerp(TransitionSource.FOV, DesiredState.FOV, Alpha);
		bTransitionActive = TransitionElapsed < ActiveTransition.Duration;
	}
	RuntimeRig->SetActorLocationAndRotation(State.PivotLocation, State.PivotRotation);
	UCameraSpringArmComponent* Boom = RuntimeRig->GetCameraBoom();
	const FCameraConfig& Config = UCameraModule::Get().GetCameraConfig();
	const bool bUseZoomSmoothing = Config.Zoom.bSmoothZoom && UserSettings.bSmoothZoom && UserSettings.ZoomSmoothing > KINDA_SMALL_NUMBER && !bTransitionActive;
	if(!bUseZoomSmoothing)
	{
		CurrentArmLength = State.ArmLength;
	}
	else
	{
		CurrentArmLength = FMath::FInterpTo(CurrentArmLength, State.ArmLength, DeltaTime, ResolveSmoothSpeed(UserSettings.ZoomSmoothing, Config.Zoom.MinSmoothSpeed, Config.Zoom.MaxSmoothSpeed));
	}
	Boom->TargetArmLength = CurrentArmLength;
	Boom->SocketOffset = State.SocketOffset;
	Boom->bDoCollisionTest = State.bEnableCollision;
	const bool bUseMovementSmoothing = Config.Movement.bSmoothMovement && UserSettings.bSmoothMovement && UserSettings.MovementSmoothing > KINDA_SMALL_NUMBER;
	const bool bUseRotationSmoothing = Config.Rotation.bSmoothRotation && UserSettings.bSmoothRotation && UserSettings.RotationSmoothing > KINDA_SMALL_NUMBER;
	Boom->CameraLagSpeed = ResolveSmoothSpeed(UserSettings.MovementSmoothing, Config.Movement.MinSmoothSpeed, Config.Movement.MaxSmoothSpeed);
	Boom->CameraRotationLagSpeed = ResolveSmoothSpeed(UserSettings.RotationSmoothing, Config.Rotation.MinSmoothSpeed, Config.Rotation.MaxSmoothSpeed);
	Boom->RefreshArm(
		DeltaTime,
		!bTransitionActive && bUseMovementSmoothing,
		!bTransitionActive && bUseRotationSmoothing);
	RuntimeRig->GetCameraComponent()->SetFieldOfView(State.FOV);
}

void ACameraManagerBase::RefreshFinalView()
{
	if(!RuntimeRig)
	{
		return;
	}

	const UCameraComponent* Camera = RuntimeRig->GetCameraComponent();
	FinalView.Transform = Camera->GetComponentTransform();
	FinalView.FOV = Camera->FieldOfView;
	FinalView.ProjectionMode = Camera->ProjectionMode;
	FinalView.OrthoWidth = Camera->OrthoWidth;
	FinalView.TrackingTarget = ActiveMode ? ActiveMode->GetTrackingTarget() : nullptr;
	FinalView.ActiveMode = ActiveMode ? ActiveMode->GetModeName() : NAME_None;
	FinalView.bTransitioning = bTransitionActive;

	if(PCOwner)
	{
		PCOwner->SetControlRotation(FinalView.Transform.Rotator());
	}
}

void ACameraManagerBase::UpdateCamera(float DeltaTime)
{
	if(ActiveDirector)
	{
		ActiveDirector->Update(DeltaTime);
	}
	if(ActiveMode)
	{
		ActiveMode->Update(DeltaTime, DesiredState);
	}
	for(UCameraFeatureBase* Feature : ActiveFeatures)
	{
		if(Feature)
		{
			Feature->Apply(DeltaTime, DesiredState);
		}
	}

	ApplyRig(DeltaTime);
	Super::UpdateCamera(DeltaTime);
	RefreshFinalView();
}

float ACameraManagerBase::ResolveSmoothSpeed(float InSmoothingAmount, float InMinSmoothSpeed, float InMaxSmoothSpeed)
{
	return FMath::Lerp(InMaxSmoothSpeed, InMinSmoothSpeed, FMath::Clamp(InSmoothingAmount, 0.f, 1.f));
}
