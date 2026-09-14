#pragma once

#include "Common/Base/WHActor.h"
#include "CameraRigBase.generated.h"

class ACameraManagerBase;
class UCameraComponent;
class UCameraSpringArmComponent;

UCLASS(NotPlaceable)
class WHFRAMEWORK_API ACameraRigBase : public AWHActor
{
	GENERATED_BODY()

public:
	ACameraRigBase();

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UCameraSpringArmComponent> CameraBoom;
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(Transient)
	TWeakObjectPtr<ACameraManagerBase> OwningCameraManager;

public:
	UCameraComponent* GetCameraComponent() const { return Camera; }
	UCameraSpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	void SetOwningCameraManager(ACameraManagerBase* InManager) { OwningCameraManager = InManager; }
	ACameraManagerBase* GetOwningCameraManager() const { return OwningCameraManager.Get(); }
};
