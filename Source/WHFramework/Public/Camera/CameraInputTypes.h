#pragma once

#include "CoreMinimal.h"
#include "CameraInputTypes.generated.h"

UENUM(BlueprintType, meta = (Bitflags))
enum class ECameraInputChannel : uint8
{
	None = 0,
	Move = 1 << 0,
	Pan = 1 << 1,
	Look = 1 << 2,
	Zoom = 1 << 3
};

ENUM_CLASS_FLAGS(ECameraInputChannel)

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraInputIntent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite)
	FVector Move = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite)
	FVector2D Look = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadWrite)
	FVector2D Pan = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadWrite)
	float Zoom = 0.f;
	UPROPERTY(BlueprintReadWrite)
	bool bBoost = false;
};
