#include "Camera/Mode/FreeCameraMode.h"

void UFreeCameraMode::AddInput(const FCameraInputIntent& Intent)
{
	MoveInput += Intent.Move;
	LookInput += Intent.Look;
	PanInput += Intent.Pan;
	ZoomInput += Intent.Zoom;
}

void UFreeCameraMode::Update(float DeltaTime, FCameraRigState& State)
{
	const FRotationMatrix Yaw(FRotator(0.f, State.PivotRotation.Yaw, 0.f));
	FVector Direction = Yaw.GetUnitAxis(EAxis::X) * MoveInput.X + Yaw.GetUnitAxis(EAxis::Y) * MoveInput.Y + FVector::UpVector * MoveInput.Z;
	State.PivotLocation += Direction.GetClampedToMaxSize(1.f) * DeltaTime;
	State.PivotLocation += (Yaw.GetUnitAxis(EAxis::Y) * PanInput.X + FVector::UpVector * PanInput.Y) * DeltaTime;
	State.PivotRotation.Yaw += LookInput.X * DeltaTime;
	State.PivotRotation.Pitch = FMath::Clamp(State.PivotRotation.Pitch + LookInput.Y * DeltaTime, -89.f, 89.f);
	State.ArmLength = FMath::Max(0.f, State.ArmLength + ZoomInput * DeltaTime);
	MoveInput = FVector::ZeroVector;
	LookInput = FVector2D::ZeroVector;
	PanInput = FVector2D::ZeroVector;
	ZoomInput = 0.f;
}
