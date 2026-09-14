#include "Camera/Components/CameraSpringArmComponent.h"

void UCameraSpringArmComponent::RefreshArm(float DeltaTime, bool bLocationLag, bool bRotationLag)
{
	UpdateDesiredArmLocation(bDoCollisionTest, bLocationLag, bRotationLag, DeltaTime);
}

void UCameraSpringArmComponent::SnapArm()
{
	UpdateDesiredArmLocation(bDoCollisionTest, false, false, 0.f);
}
