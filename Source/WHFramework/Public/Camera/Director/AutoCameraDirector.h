#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Camera/Director/CameraDirectorBase.h"
#include "AutoCameraDirector.generated.h"
class ACameraShotAnchor;
UCLASS()
class WHFRAMEWORK_API UAutoCameraDirector : public UCameraDirectorBase
{
	GENERATED_BODY()
public:
	virtual void Start(ACameraManagerBase* InManager) override;
	virtual void Stop() override;
	virtual void Update(float DeltaTime) override;
	float ScoreCandidate(const ACameraShotAnchor* Anchor) const;
private:
	TWeakObjectPtr<ACameraShotAnchor> CurrentAnchor;

	FCameraConfigOverrideHandle CurrentShotOverrideHandle;

	TMap<TWeakObjectPtr<ACameraShotAnchor>, float> Cooldowns;
	float ShotElapsed = 0.f;
	float ShotDuration = 0.f;
	bool IsOnCooldown(const ACameraShotAnchor* Anchor) const;
	void SelectNextShot();
};
