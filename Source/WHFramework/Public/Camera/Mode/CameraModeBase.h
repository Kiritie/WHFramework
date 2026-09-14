#pragma once

#include "Camera/CameraModuleTypes.h"
#include "CameraModeBase.generated.h"

class ACameraManagerBase;

UCLASS(Abstract, Blueprintable)
class WHFRAMEWORK_API UCameraModeBase : public UObject
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient)
	TObjectPtr<ACameraManagerBase> CameraManager;

public:
	virtual FName GetModeName() const PURE_VIRTUAL(UCameraModeBase::GetModeName, return NAME_None;);

	virtual void Enter(ACameraManagerBase* InManager, const FCameraModeContext& Context);

	virtual void Exit();

	virtual void Update(float DeltaTime, FCameraRigState& InOutState);

	virtual void AddInput(const FCameraInputIntent& Intent);

	virtual AActor* GetTrackingTarget() const { return nullptr; }

	virtual const FCameraTargetParams* GetTargetParams() const { return nullptr; }
};
