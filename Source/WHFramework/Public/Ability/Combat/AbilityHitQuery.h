#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "CollisionShape.h"
#include "AbilityHitQuery.generated.h"

class UWorld;

UENUM(BlueprintType)
enum class EAbilityHitQueryMode : uint8
{
	None = 0,
	Ray,
	Sweep,
	Overlap,
	Trajectory
};

UENUM(BlueprintType)
enum class EAbilityHitShape : uint8
{
	None = 0,
	Sphere,
	Capsule,
	Box
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityHitShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilityHitShape Type = EAbilityHitShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1"))
	float Radius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1"))
	float HalfHeight = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector HalfExtent = FVector(20.f);

	bool IsValid() const;
	FCollisionShape MakeCollisionShape() const;
	float GetBoundingRadius() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityHitTrajectoryKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1"))
	float Alpha = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocalStart = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocalEnd = FVector(100.f, 0.f, 0.f);
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityHitQueryDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilityHitQueryMode Mode = EAbilityHitQueryMode::Sweep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityHitShape Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocalStart = FVector(30.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocalEnd = FVector(180.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator LocalRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 HitGroup = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	float MagnitudeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bContinuous = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseAimDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityHitTrajectoryKey> Trajectory;

	bool Validate(float MaximumRange, FString& OutError) const;
	void EvaluateTrajectory(float Alpha, FVector& OutStart, FVector& OutEnd) const;
};

struct WHFRAMEWORK_API FAbilityHitQueryContext
{
	UWorld* World = nullptr;
	TArray<TEnumAsByte<ECollisionChannel>> TargetObjectChannels;
	ECollisionChannel BlockingChannel = ECC_MAX;
	TArray<TWeakObjectPtr<AActor>> IgnoredActors;
	TFunction<bool(AActor*)> IsTargetActor;
	int32 RemainingQueries = 512;
};

class WHFRAMEWORK_API FAbilityHitQuery
{
public:
	static bool Trace(FAbilityHitQueryContext& Context, const FVector& Start, const FVector& End,
		const FQuat& Rotation, const FAbilityHitShape* Shape, TArray<FHitResult>& OutHits, FHitResult& OutBlocker);
	static bool HasLineOfSight(FAbilityHitQueryContext& Context, const FVector& Start, AActor* Target,
		const FVector& End, bool& bOutVisible);
	static bool Gather(FAbilityHitQueryContext& Context, const FAbilityHitQueryDefinition& Definition,
		const FTransform& PreviousFrame, const FTransform& CurrentFrame, float PreviousAlpha, float CurrentAlpha,
		TArray<FHitResult>& OutHits);
};
