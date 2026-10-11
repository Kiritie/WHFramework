#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Components/CharacterMovementTypes.h"
#include "CharacterMovementComponentBase.generated.h"

class ICharacterMovementEnvironmentInterface;
class FSavedMove_CharacterBase;

/** 移动模式、来源约束及网络移动统一由 CMC 持有。 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UCharacterMovementComponentBase : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_CharacterBase;

public:
	UCharacterMovementComponentBase(const FObjectInitializer& InObjectInitializer = FObjectInitializer::Get());
	virtual void EndPlay(const EEndPlayReason::Type InReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual void UpdateFromCompressedFlags(uint8 InFlags) override;
	virtual void PhysicsVolumeChanged(APhysicsVolume* InNewVolume) override;
	virtual void OnMovementModeChanged(EMovementMode InPreviousMode, uint8 InPreviousCustomMode) override;
	virtual void OnMovementUpdated(float InDeltaSeconds, const FVector& InOldLocation, const FVector& InOldVelocity) override;

	bool SetEnvironmentProvider(UObject* InProvider);
	bool SetMovementConstraint(const FGuid& InSourceId, UObject* InSource, const FCharacterMovementConstraint& InConstraint);
	bool ApplyMovementForce(const FGuid& InSourceId, UObject* InSource, const FVector& InVelocity, float InDuration, bool bInAdditive);
	bool ReleaseMovementSource(const FGuid& InSourceId, UObject* InSource);
	void ResetMovementSources();
	void ResetFallTracking();
	ECharacterWorldQueryResult ResumeMovementFromLifecycle();
	void SetBaseMovementSettings(float InWalkSpeed, float InSwimSpeed, float InFlySpeed, float InRotationSpeed, float InJumpVelocity);
	bool IsClimbing() const;
	bool IsFloating() const;
	bool IsSprinting() const;
	bool IsWaitingForTerrain() const;
	const FCharacterMovementConstraint& GetMovementConstraint() const;
	FOnCharacterMovementLanded OnMovementLanded;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float InDeltaSeconds) override;
	virtual void PhysCustom(float InDeltaSeconds, int32 InIterations) override;
	virtual void PhysSwimming(float InDeltaSeconds, int32 InIterations) override;
	virtual void PhysFalling(float InDeltaSeconds, int32 InIterations) override;
	virtual void PhysWalking(float InDeltaSeconds, int32 InIterations) override;
	virtual void PhysNavWalking(float InDeltaSeconds, int32 InIterations) override;
	virtual void PhysFlying(float InDeltaSeconds, int32 InIterations) override;
	virtual void PhysicsRotation(float InDeltaSeconds) override;
	virtual void ProcessLanded(const FHitResult& InHit, float InRemainingTime, int32 InIterations) override;
	virtual bool MoveUpdatedComponentImpl(const FVector& InDelta, const FQuat& InRotation, bool bInSweep, FHitResult* OutHit, ETeleportType InTeleport) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float SprintMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0"))
	float ClimbSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float WaterFriction = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float FloatResponse = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0"))
	float MaximumForceSpeed = 5000.f;

private:
	struct FMovementSource
	{
		TWeakObjectPtr<UObject> Owner;
		FCharacterMovementConstraint Constraint;
		uint16 RootMotionId = 0;
	};

	bool CanAcceptSource(const FGuid& InSourceId, const UObject* InSource) const;
	void PruneMovementSources();
	void RebuildConstraint();
	FCharacterMovementConstraint CaptureMovementConstraint() const;
	FCharacterMovementConstraint GetSimulationConstraint() const;
	ICharacterMovementEnvironmentInterface* GetEnvironmentProvider() const;
	void RefreshEnvironment();
	void SelectEnvironmentMovement();
	void PhysClimbing(float InDeltaSeconds, int32 InIterations);
	void PhysWater(float InDeltaSeconds, int32 InIterations, bool bInFloat);
	void MoveWithCollision(float InDeltaSeconds);
	FBox MakeMovementBounds(const FVector& InLocation) const;

	TWeakObjectPtr<UObject> EnvironmentProvider;
	TMap<FGuid, FMovementSource> Sources;
	UPROPERTY(Replicated)
	FCharacterMovementConstraint AggregatedConstraint;
	FCharacterMovementConstraint ReplayConstraint;
	FCharacterMovementEnvironment Environment;
	bool bUsingReplayConstraint = false;
	bool bNetworkSprintIntent = false;
	bool bNetworkClimbIntent = false;
	bool bNetworkFlyIntent = false;
	bool bWaitingForTerrain = false;
	bool bTrackingFall = false;
	double FallApexZ = 0.0;
	float BaseRotationSpeed = 360.f;
	float ConfiguredStepHeight = -1.f;
	uint64 ObservedWorldEpoch = 0;
};
