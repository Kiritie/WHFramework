#pragma once

#include "GameFramework/SpringArmComponent.h"
#include "CameraSpringArmComponent.generated.h"

UCLASS(ClassGroup = Camera)
class WHFRAMEWORK_API UCameraSpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	void RefreshArm(float DeltaTime, bool bLocationLag, bool bRotationLag);
	void SnapArm();
};
