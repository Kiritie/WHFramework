#pragma once

#include "CoreMinimal.h"
#include "CharacterMovementTypes.generated.h"

struct FHitResult;

UENUM(BlueprintType)
enum class ECharacterWorldQueryResult : uint8
{
	None,
	Ready,
	WaitingTerrain,
	Invalid
};

UENUM(BlueprintType)
enum class ECharacterTraversalMode : uint8
{
	None,
	Climb,
	Fly,
	Glide
};

UENUM(BlueprintType)
enum class ECharacterCustomMovementMode : uint8
{
	None,
	Climb,
	Float,
	Glide
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterMovementConstraint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RotationMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBlockMovement = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBlockRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSprint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECharacterTraversalMode Traversal = ECharacterTraversalMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Priority = 0;

	bool IsValid() const;
	bool Matches(const FCharacterMovementConstraint& InOther) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterMovementEnvironment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ECharacterWorldQueryResult Result = ECharacterWorldQueryResult::None;

	UPROPERTY(BlueprintReadOnly)
	bool bBodyInWater = false;

	UPROPERTY(BlueprintReadOnly)
	bool bHeadUnderWater = false;

	UPROPERTY(BlueprintReadOnly)
	float WaterSurfaceZ = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float FloatCenterZ = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bClimbable = false;

	UPROPERTY(BlueprintReadOnly)
	FVector ClimbNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float StepHeight = -1.f;

	uint64 WorldEpoch = 0;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCharacterMovementLanded, const FHitResult&, float, float);
