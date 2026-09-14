#include "Camera/Mode/FollowCameraMode.h"
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
	Rotation.Pitch += RuntimePitchOffset;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch, Resolved.PitchRange.X, Resolved.PitchRange.Y);
	State.PivotRotation = Rotation;
	State.ArmLength = FMath::Clamp(Resolved.Distance + RuntimeDistanceOffset, Resolved.DistanceRange.X, Resolved.DistanceRange.Y);
	State.SocketOffset = Resolved.SocketOffset;
	State.FOV = Resolved.FOV;
	State.bEnableCollision = Resolved.bEnableCollision;
}
