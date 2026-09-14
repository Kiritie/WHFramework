#pragma once
#include "Camera/Mode/CameraModeBase.h"
#include "FreeCameraMode.generated.h"

UCLASS()
class WHFRAMEWORK_API UFreeCameraMode : public UCameraModeBase
{
	GENERATED_BODY()
public:
	virtual FName GetModeName() const override { return TEXT("Free"); }
	virtual void AddInput(const FCameraInputIntent& Intent) override;
	virtual void Update(float DeltaTime, FCameraRigState& InOutState) override;
private:
	FVector MoveInput = FVector::ZeroVector;
	FVector2D LookInput = FVector2D::ZeroVector;
	FVector2D PanInput = FVector2D::ZeroVector;
	float ZoomInput = 0.f;
};
