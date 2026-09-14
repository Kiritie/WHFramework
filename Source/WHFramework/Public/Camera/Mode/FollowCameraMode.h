#pragma once

#include "Camera/Mode/CameraModeBase.h"
#include "FollowCameraMode.generated.h"

UCLASS()
class WHFRAMEWORK_API UFollowCameraMode : public UCameraModeBase
{
	GENERATED_BODY()

public:
	virtual FName GetModeName() const override { return TEXT("Follow"); }

	virtual void Enter(ACameraManagerBase* InManager, const FCameraModeContext& Context) override;

	virtual void Update(float DeltaTime, FCameraRigState& InOutState) override;

	virtual void AddInput(const FCameraInputIntent& Intent) override;

	virtual AActor* GetTrackingTarget() const override { return Target.Get(); }

	virtual const FCameraTargetParams* GetTargetParams() const override { return &TargetParams; }

private:
	TWeakObjectPtr<AActor> Target;

	FCameraTargetParams TargetParams;

	FRotator InitialTargetRotation = FRotator::ZeroRotator;

	FVector2D PendingLook = FVector2D::ZeroVector;

	float PendingZoom = 0.f;

	float RuntimeYawOffset = 0.f;

	float RuntimePitchOffset = 0.f;

	float RuntimeDistanceOffset = 0.f;
};
