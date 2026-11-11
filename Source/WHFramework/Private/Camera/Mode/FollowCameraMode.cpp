#include "Camera/Mode/FollowCameraMode.h"
#include "Camera/CameraInputTypes.h"
#include "Camera/CameraViewTypes.h"
#include "Camera/CameraConfigTypes.h"
#include "Camera/CameraFeatureTypes.h"
#include "Camera/Interface/CameraTrackableInterface.h"
#include "Camera/Manager/CameraManagerBase.h"

void UFollowCameraMode::Enter(ACameraManagerBase* InManager, const FCameraModeContext& Context)
{
	Super::Enter(InManager, Context);

	Target = Context.Target;
	if(AActor* TargetActor = Target.Get())
	{
		InitialTargetRotation = TargetActor->GetActorRotation();
		if(TargetActor->Implements<UCameraTrackableInterface>())
		{
			TargetParams = ICameraTrackableInterface::Execute_GetCameraTargetParams(TargetActor);
		}
	}
}

void UFollowCameraMode::AddInput(const FCameraInputIntent& Intent)
{
	PendingLook += Intent.Look;
	PendingZoom += Intent.Zoom;
}

void UFollowCameraMode::Update(float DeltaTime, FCameraRigState& State)
{
	AActor* Actor = Target.Get();
	if(!Actor || !CameraManager)
	{
		return;
	}

	const FCameraResolvedConfig& Resolved = CameraManager->GetResolvedCameraConfig();
	RuntimeYawOffset += PendingLook.X * DeltaTime;
	RuntimePitchOffset += PendingLook.Y * DeltaTime;
	RuntimeDistanceOffset += PendingZoom * DeltaTime;
	PendingLook = FVector2D::ZeroVector;
	PendingZoom = 0.f;

	State.PivotLocation = Actor->GetActorTransform().TransformPositionNoScale(Resolved.PivotOffset);
	const FRotator BaseRotation = TargetParams.bInheritTargetRotation ? Actor->GetActorRotation() : InitialTargetRotation;
	FRotator Rotation = BaseRotation + Resolved.RotationOffset;
	Rotation.Yaw += RuntimeYawOffset;
	const float BasePitch = Rotation.Pitch;
	Rotation.Pitch = FMath::Clamp(BasePitch + RuntimePitchOffset, Resolved.PitchRange.X, Resolved.PitchRange.Y);
	RuntimePitchOffset = Rotation.Pitch - BasePitch;
	State.PivotRotation = Rotation;
	State.ArmLength = FMath::Clamp(Resolved.Distance + RuntimeDistanceOffset, Resolved.DistanceRange.X, Resolved.DistanceRange.Y);
	RuntimeDistanceOffset = State.ArmLength - Resolved.Distance;
	State.SocketOffset = Resolved.SocketOffset;
	if(!Resolved.bSocketOffsetOverridden)
	{
		if(const ICameraTrackableInterface* Trackable = Cast<ICameraTrackableInterface>(Actor))
		{
			State.SocketOffset = Trackable->ResolveFollowCameraSocketOffset(State.SocketOffset, State.ArmLength);
		}
	}
	State.FOV = Resolved.FOV;
	State.bEnableCollision = Resolved.bEnableCollision;
}
