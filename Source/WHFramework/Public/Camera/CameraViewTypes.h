#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "Math/MathTypes.h"
#include "CameraViewTypes.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraRigState
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector PivotLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator PivotRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArmLength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SocketOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FOV = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableCollision = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableLocationLag = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableRotationLag = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LocationLagSpeed = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RotationLagSpeed = 10.f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraViewSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	FTransform Transform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly)
	float FOV = 90.f;
	UPROPERTY(BlueprintReadOnly)
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode = ECameraProjectionMode::Perspective;
	UPROPERTY(BlueprintReadOnly)
	float OrthoWidth = 0.f;
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> TrackingTarget = nullptr;
	UPROPERTY(BlueprintReadOnly)
	FName ActiveMode = NAME_None;
	UPROPERTY(BlueprintReadOnly)
	bool bTransitioning = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraTransitionParams
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInstant = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEaseType EaseType = EEaseType::Linear;
	static FCameraTransitionParams Instant()
	{
		FCameraTransitionParams Result;
		Result.bInstant = true;
		return Result;
	}
	static FCameraTransitionParams Smooth(float InDuration = 0.35f, EEaseType InEase = EEaseType::InOutSine)
	{
		FCameraTransitionParams Result;
		Result.Duration = InDuration;
		Result.EaseType = InEase;
		return Result;
	}
};
