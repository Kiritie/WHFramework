#include "Camera/Mode/FixedCameraMode.h"
#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/Manager/CameraManagerBase.h"

void UFixedCameraMode::Enter(ACameraManagerBase* InManager, const FCameraModeContext& Context)
{
	Super::Enter(InManager, Context);
	Anchor = Context.Anchor;
}

void UFixedCameraMode::Update(float DeltaTime, FCameraRigState& InOutState)
{
	const ACameraShotAnchor* AnchorActor = Anchor.Get();
	if(!AnchorActor)
	{
		return;
	}
	const FCameraResolvedConfig& Resolved = CameraManager->GetResolvedCameraConfig();
	InOutState.PivotLocation = AnchorActor->GetActorTransform().TransformPositionNoScale(Resolved.PivotOffset);
	InOutState.PivotRotation = AnchorActor->GetActorRotation() + Resolved.RotationOffset;
	InOutState.ArmLength = FMath::Clamp(Resolved.Distance, Resolved.DistanceRange.X, Resolved.DistanceRange.Y);
	InOutState.FOV = Resolved.FOV;
	InOutState.SocketOffset = Resolved.SocketOffset;
	InOutState.bEnableCollision = Resolved.bEnableCollision;
	InOutState.bEnableLocationLag = false;
	InOutState.bEnableRotationLag = false;
}
