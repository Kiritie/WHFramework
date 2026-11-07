#pragma once

#include "CoreMinimal.h"
#include "CameraConfigTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraUserSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MovementSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableVerticalPan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HorizontalLookSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalLookSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInvertHorizontalLook = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInvertVerticalLook = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HorizontalPanSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalPanSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInvertHorizontalPan = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInvertVerticalPan = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothMovement", ClampMin = "0.0", ClampMax = "1.0"))
	float MovementSmoothing = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothRotation", ClampMin = "0.0", ClampMax = "1.0"))
	float RotationSmoothing = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothZoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothZoom", ClampMin = "0.0", ClampMax = "1.0"))
	float ZoomSmoothing = 0.5f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraMovementConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HorizontalPanSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalPanSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"))
	float BoostMultiplier = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableVerticalPan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothMovement", ClampMin = "0.01"))
	float MinSmoothSpeed = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothMovement", ClampMin = "0.01"))
	float MaxSmoothSpeed = 25.f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraRotationConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HorizontalSpeed = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float VerticalSpeed = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D PitchRange = FVector2D(-89.f, 89.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothRotation", ClampMin = "0.01"))
	float MinSmoothSpeed = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothRotation", ClampMin = "0.01"))
	float MaxSmoothSpeed = 25.f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraZoomConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Speed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D DistanceRange = FVector2D(0.f, 1000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothZoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothZoom", ClampMin = "0.01"))
	float MinSmoothSpeed = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bSmoothZoom", ClampMin = "0.01"))
	float MaxSmoothSpeed = 25.f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraMovementConfig Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraRotationConfig Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraZoomConfig Zoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DefaultPivotOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DefaultSocketOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator DefaultRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefaultDistance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefaultFOV = 90.f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraTargetParams
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector PivotOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SocketOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Distance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideDistanceRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideDistanceRange"))
	FVector2D DistanceRange = FVector2D(0.f, 1000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverridePitchRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverridePitchRange"))
	FVector2D PitchRange = FVector2D(-89.f, 89.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInheritTargetRotation = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraConfigOverride
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverridePivotOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverridePivotOffset"))
	FVector PivotOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideSocketOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideSocketOffset"))
	FVector SocketOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideRotationOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideRotationOffset"))
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideDistance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideDistance"))
	float Distance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideDistanceRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideDistanceRange"))
	FVector2D DistanceRange = FVector2D(0.f, 1000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverridePitchRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverridePitchRange"))
	FVector2D PitchRange = FVector2D(-89.f, 89.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideFOV = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideFOV"))
	float FOV = 90.f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraConfigOverrideHandle
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	FGuid ID;

	bool IsValid() const { return ID.IsValid(); }

	void Reset() { ID.Invalidate(); }
};

USTRUCT()
struct WHFRAMEWORK_API FCameraConfigOverrideEntry
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FCameraConfigOverrideHandle Handle;

	UPROPERTY()
	int32 Priority = 0;

	UPROPERTY()
	uint64 Sequence = 0;

	UPROPERTY()
	FCameraConfigOverride Data;
};

USTRUCT()
struct WHFRAMEWORK_API FCameraResolvedConfig
{
	GENERATED_BODY()

public:
	FVector PivotOffset = FVector::ZeroVector;

	FVector SocketOffset = FVector::ZeroVector;

	bool bSocketOffsetOverridden = false;

	FRotator RotationOffset = FRotator::ZeroRotator;

	float Distance = 300.f;

	FVector2D DistanceRange = FVector2D(0.f, 1000.f);

	FVector2D PitchRange = FVector2D(-89.f, 89.f);

	float FOV = 90.f;

	bool bEnableCollision = true;
};
