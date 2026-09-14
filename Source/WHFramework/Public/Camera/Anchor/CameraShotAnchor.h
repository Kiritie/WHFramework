#pragma once

#include "Camera/CameraModuleTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "CameraShotAnchor.generated.h"

class UCameraModeBase;

UCLASS()
class WHFRAMEWORK_API ACameraShotAnchor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer ShotTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseScore = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MinDuration = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxDuration = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Cooldown = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FCameraConfigOverride CameraOverride;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCameraModeBase> ModeClass;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
