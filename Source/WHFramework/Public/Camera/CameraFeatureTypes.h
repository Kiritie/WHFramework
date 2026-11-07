#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraViewTypes.h"
#include "CameraFeatureTypes.generated.h"

class AActor;
class ACameraShotAnchor;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraModeContext
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> Target = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<ACameraShotAnchor> Anchor = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraTransitionParams Transition;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraFeatureHandle
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	FGuid Id;
	bool IsValid() const { return Id.IsValid(); }
	bool operator==(const FCameraFeatureHandle& Other) const { return Id == Other.Id; }
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraFeatureContext
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Priority = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraTransitionParams EnterTransition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraTransitionParams ExitTransition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScalarValue = 0.f;
};
