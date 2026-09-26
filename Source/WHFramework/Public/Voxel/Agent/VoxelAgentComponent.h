#pragma once

#include "Components/ActorComponent.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Interaction/VoxelEditTypes.h"

#include "VoxelAgentComponent.generated.h"

class ACharacter;
class APlayerController;
class UCharacterMovementComponent;
class UVoxelModule;
class UVoxelModuleNetworkComponent;

UCLASS(
	ClassGroup = (Voxel),
	meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UVoxelAgentComponent :
	public UActorComponent
{
	GENERATED_BODY()

public:
	UVoxelAgentComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type InReason) override;

	virtual void TickComponent(
		float InDeltaSeconds,
		ELevelTick InTickType,
		FActorComponentTickFunction* InTickFunction) override;

	UFUNCTION(BlueprintCallable)
	void SetAgentEnabled(
		bool bInEnabled);

	UFUNCTION(BlueprintCallable)
	void BindController(
		APlayerController* InController);

	void NotifyTravelTeleported();
	bool IsTravelObserverReady() const;

	UFUNCTION(BlueprintCallable)
	bool TraceVoxel(
		FVoxelHitResult& OutHit) const;

	UFUNCTION(BlueprintCallable)
	void BeginBreak();

	UFUNCTION(BlueprintCallable)
	void EndBreak();

	UFUNCTION(BlueprintCallable)
	bool PlaceSelected();

	UFUNCTION(BlueprintCallable)
	bool PlaceItem(
		const FVoxelItem& InItem);

	UFUNCTION(BlueprintCallable)
	bool UseTarget();

	UFUNCTION(BlueprintCallable)
	bool TakeContainerSlot(
		int32 InContainerSlot,
		int32 InCount);

	UFUNCTION(BlueprintCallable)
	bool PutContainerSlot(
		int32 InContainerSlot,
		int32 InCount);

public:
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming")
	bool bEnableStreaming = true;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming")
	bool bEnableNonPlayerSource = false;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming")
	bool bUseControllerView = true;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0"))
	int32 ExactRadiusCells = 64;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0"))
	int32 CollisionRadiusCells = 32;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0"))
	int32 SimulationRadiusCells = 0;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0"))
	int32 VerticalExactRadiusCells = 16;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Streaming",
		meta = (ClampMin = "0"))
	int32 MovementCriticalCollisionRadiusCells = 12;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Interaction")
	bool bCreativeInStandalone = false;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Movement")
	bool bGateInitialSpawnUntilCollision = true;

	UPROPERTY(
		EditAnywhere,
		Category = "Voxel|Movement")
	bool bUseSmallVoxelStepHeight = false;

	UPROPERTY(
		EditAnywhere,
		Category = "Voxel|Movement",
		meta = (ClampMin = "0.01"))
	float CollisionPredictionSeconds = 0.10f;

private:
	bool View(
		FVector& OutOrigin,
		FVector& OutDirection) const;

	bool MakeIntent(
		EVoxelEditAction InAction,
		FVoxelEditIntent& OutIntent) const;

	bool Send(
		FVoxelEditIntent InIntent);

	void RefreshSource();
	void RefreshStepHeight();
	void RefreshCollisionMovementGuard();

	void ResetInitialCollisionGate();
	void ApplyInitialCollisionGate(
		ACharacter& InCharacter,
		UCharacterMovementComponent& InMovement);

	void ReleaseInitialCollisionGate(
		ACharacter& InCharacter,
		UCharacterMovementComponent& InMovement);

	bool IsCollisionReadyAt(
		const ACharacter& InCharacter,
		const FVector& InLocation) const;

	void OnReply(
		const FVoxelEditReply& InReply);

private:
	TWeakObjectPtr<APlayerController> BoundController;
	TWeakObjectPtr<UVoxelModule> Module;
	TWeakObjectPtr<UVoxelModuleNetworkComponent> Network;

	FGuid SourceId;
	FDelegateHandle ReplyHandle;

	bool bHeld = false;
	bool bBreakActive = false;

	/**
	 * 只允许 Initial Spawn 使用 MOVE_None。
	 */
	bool bInitialCollisionGatePending = true;
	bool bInitialCollisionGateApplied = false;

	uint8 InitialPreviousMoveMode = 0;
	uint8 InitialPreviousCustomMode = 0;

	bool bHasLastSafeLocation = false;
	FVector LastSafeLocation = FVector::ZeroVector;

	uint64 ObservedWorldEpoch = 0;

	float PreviousStepHeight = -1.0f;

	FIntVector BreakTarget = FIntVector::ZeroValue;
	double LastPulse = -10.0;
	uint64 NextLocalID = 1;
	uint64 LastRequest = 0;
};
