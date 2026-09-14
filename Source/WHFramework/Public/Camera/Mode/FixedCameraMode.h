#pragma once

#include "Camera/Mode/CameraModeBase.h"
#include "FixedCameraMode.generated.h"

class ACameraShotAnchor;

UCLASS()
class WHFRAMEWORK_API UFixedCameraMode : public UCameraModeBase
{
	GENERATED_BODY()

public:
	virtual FName GetModeName() const override { return TEXT("Fixed"); }
	virtual void Enter(ACameraManagerBase* InManager, const FCameraModeContext& Context) override;
	virtual void Update(float DeltaTime, FCameraRigState& InOutState) override;

private:
	TWeakObjectPtr<ACameraShotAnchor> Anchor;
};
