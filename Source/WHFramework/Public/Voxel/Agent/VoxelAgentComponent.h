#pragma once

#include "Components/ActorComponent.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Interaction/VoxelEditTypes.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "Voxel/VoxelWorldReadinessRuntime.h"

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
	bool IsStreamingLocationAdmitted(const FIntVector& InCell, uint64 InEpoch) const;
	bool BuildStreamingSource(bool bInLocalView, FVoxelStreamingSource& OutSource) const;
	bool PrepareBirthSource(const FGuid& InSource);
	bool TryCommitBirthSource();
	bool IsBirthSourceCommitted() const;
	const FVoxelPlayerClaimToken& GetBirthClaimToken() const { return BirthClaim; }
	bool IsBirthCandidateCommitted() const { return bBirthCandidateCommitted; }
	void SetBirthCandidateCommitted(bool bInCommitted) { bBirthCandidateCommitted = bInCommitted; }
	const FGuid& GetStreamingSourceId() const { return SourceId; }

	UFUNCTION(BlueprintCallable)
	bool TraceVoxel(
		FVoxelHitResult& OutHit) const;

	UFUNCTION(BlueprintCallable)
	void BeginBreak();

	UFUNCTION(BlueprintCallable)
	void EndBreak();

	UFUNCTION(BlueprintCallable)
	bool PlaceSelected();

	bool CanRotateSelected() const;
	bool RotateSelected(int32 InQuarterTurns);
	void ResetPlacementRotation();
	uint8 GetPlacementQuarterTurns() const { return PlacementQuarterTurns; }
	FSimpleMulticastDelegate OnPlacementChanged;

	UFUNCTION(BlueprintCallable)
	bool PlaceItem(
		const FVoxelItem& InItem);

	UFUNCTION(BlueprintCallable)
	bool UseTarget();



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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming")
	bool bInheritWorldView = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (EditCondition = "!bInheritWorldView"))
	FVoxelStreamingSourceView StreamingView;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ToolTip = "Request world Macro coverage in addition to this source's Fine, Proxy and Surface ranges."))
	bool bProvideWorldView = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Streaming", meta = (ToolTip = "Smaller values are scheduled first."))
	int32 StreamingPriority = 0;

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
	int32 MovementCriticalCollisionRadiusCells = 12;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Voxel|Interaction")
	bool bCreativeInStandalone = false;


private:
	bool View(
		FVector& OutOrigin,
		FVector& OutDirection) const;

	bool MakeIntent(
		EVoxelEditAction InAction,
		FVoxelEditIntent& OutIntent) const;
	FGuid GetInventoryStoreId() const;

	bool Send(
		FVoxelEditIntent InIntent);

	void RefreshSource();
	void ResetBirthClaim();

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
	FGuid BirthPawnIncarnation = FGuid::NewGuid();
	FVoxelPlayerClaimToken BirthClaim;
	bool bBirthCandidateCommitted = true;
	FDelegateHandle ReplyHandle;

	bool bHeld = false;
	bool bBreakActive = false;
	uint8 PlacementQuarterTurns = 0;

	uint64 ObservedWorldEpoch = 0;

	FIntVector BreakTarget = FIntVector::ZeroValue;
	double LastPulse = -10.0;
	uint64 NextLocalID = 1;
	uint64 LastRequest = 0;
};
