#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelFineBuildState.h"

enum class EVoxelFineOwnerKind : uint8
{
	None = 0,
	F0,
	F1Plus,
	Proxy,
	Surface,
	Macro
};

struct WHFRAMEWORK_API FVoxelFineOwner
{
	EVoxelFineOwnerKind Kind = EVoxelFineOwnerKind::None;
	FGuid Identity;
	uint64 Incarnation = 0;

	bool operator==(const FVoxelFineOwner& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelFineContentIdentity
{
	uint64 WorldEpoch = 0;
	uint64 GeneratorIdentity = 0;
	uint64 RecipeRevision = 0;
	uint64 RegistryRevision = 0;
	uint64 ShapeRevision = 0;
	uint64 DataRevision = 0;
	uint64 OverlayRevision = 0;
	uint64 MeshRevision = 0;
	FGuid GeometryIdentity;
	int32 SampleStepCells = 0;

	bool operator==(const FVoxelFineContentIdentity& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelFineOwnerEvidence
{
	FVoxelFineOwner Owner;
	FVoxelFineContentIdentity Content;
	bool bGeometryReady = false;
	bool bKnownEmpty = false;
	bool bCanStage = false;
	TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> GeometryLease;
};

struct WHFRAMEWORK_API FVoxelFineOwnershipChange
{
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelFineOwner Before;
	uint64 BeforeGeneration = 0;
	FVoxelFineOwnerEvidence After;
	bool bRequiresCollision = false;
};

struct WHFRAMEWORK_API FVoxelFineHandoffTransaction
{
	FGuid Id;
	uint64 WorldEpoch = 0;
	uint64 DemandRevision = 0;
	TArray<FVoxelFineOwnershipChange> Changes;
};

struct WHFRAMEWORK_API FVoxelFinePublishStamp
{
	uint64 WorldEpoch = 0;
	uint64 PresentationSerial = 0;
	uint64 FrameCounter = 0;

	bool operator==(const FVoxelFinePublishStamp& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelFineFrozenHandoff
{
	FVoxelFineHandoffTransaction Transaction;
	FVoxelFinePublishStamp Stamp;
	TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> PublicationLease;
};

struct WHFRAMEWORK_API FVoxelFineRenderReceiptAtom
{
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelFineOwner Owner;
	uint64 OwnerGeneration = 0;
	FVoxelFineContentIdentity Content;
	bool bKnownEmpty = false;
};

struct WHFRAMEWORK_API FVoxelFineRenderReceipt
{
	FGuid TransactionId;
	FVoxelFinePublishStamp Stamp;
	TArray<FVoxelFineRenderReceiptAtom> Atoms;
};

struct WHFRAMEWORK_API FVoxelFinePresentedAtom
{
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelFineOwnerEvidence Evidence;
	uint64 OwnerGeneration = 0;
	FVoxelFinePublishStamp Stamp;
	FGuid TransactionId;
};

enum class EVoxelFineReceiptDisposition : uint8
{
	None = 0,
	Applied,
	Duplicate,
	Superseded,
	Invalid,
	Unknown,
	WorldExpired
};

enum class EVoxelFineCancelDisposition : uint8
{
	None = 0,
	CanceledBeforeSeal,
	RecordedAfterSeal,
	AlreadySettled,
	Unknown
};

// 唯一逐 Atom Presented 状态；封存之后只结算实际回执，不重新比较 Desired。
class WHFRAMEWORK_API FVoxelFineHandoff
{
public:
	explicit FVoxelFineHandoff(uint64 InWorldEpoch);
	void SetSectionChangedCallback(TFunction<void(const FIntVector&)> InCallback);
	bool Prepare(TConstArrayView<FVoxelFineOwnershipChange> InChanges, uint64 InDemandRevision,
		FVoxelFineHandoffTransaction& OutTransaction, FString& OutError);
	bool ValidateBeforeSeal(const FGuid& InTransaction,
		TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsEvidenceCurrent,
		TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsCollisionCurrent,
		FString& OutError) const;
	bool SealTransaction(const FGuid& InTransaction, const FVoxelFinePublishStamp& InStamp,
		TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> InPublicationLease,
		TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsEvidenceCurrent,
		TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsCollisionCurrent,
		TFunctionRef<bool(const FVoxelFineHandoffTransaction&, const FVoxelTaskCustomPayload&)> InAreInterfacesCurrent,
		TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe>& OutFrozen, FString& OutError);
	EVoxelFineReceiptDisposition CommitRenderReceipt(const FVoxelFineRenderReceipt& InReceipt,
		TArray<FVoxelFinePresentedAtom>& OutRetirement, FString& OutError);
	EVoxelFineCancelDisposition RequestCancel(const FGuid& InTransaction,
		TOptional<FVoxelFineHandoffTransaction>& OutCanceledBeforeSeal);
	const FVoxelFineOwner* FindOwner(const FIntVector& InSection) const;
	const FVoxelFinePresentedAtom* FindPresentedAtom(const FIntVector& InSection) const;
	bool IsLocked(const FIntVector& InSection) const;
	bool CanReleaseF0(const FIntVector& InSection, bool bInRuntimeDemanded, bool bInCollisionDemanded,
		bool bInSimulationDemanded, bool bInTaskPinned, bool bInPersistenceDirty,
		TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsReplacementCurrent) const;
	const FVoxelFineHandoffTransaction* FindTransaction(const FGuid& InId) const;
	bool WasCancelRequested(const FGuid& InId) const;
	void ForgetSettledReceipt(const FGuid& InId);
	void DrainForShutdown(uint64 InNewWorldEpoch, TArray<FVoxelFineHandoffTransaction>& OutPrepared,
		TArray<TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe>>& OutFrozen,
		TArray<FVoxelFinePresentedAtom>& OutPresented);

private:
	struct FTransactionRecord
	{
		FVoxelFineHandoffTransaction Transaction;
		TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe> Frozen;
		bool bCancelRequested = false;
	};

	bool ValidateEvidence(const FVoxelFineOwnerEvidence& InEvidence) const;
	void ReleaseLocks(const FVoxelFineHandoffTransaction& InTransaction);
	uint64 WorldEpoch = 0;
	uint64 HighestSealedPresentationSerial = 0;
	uint64 HighestSealedFrameCounter = 0;
	TMap<FIntVector, FVoxelFinePresentedAtom> PresentedAtoms;
	TMap<FIntVector, FGuid> Locks;
	TMap<FGuid, FTransactionRecord> Transactions;
	TMap<FGuid, FVoxelFinePublishStamp> SettledReceipts;
	TFunction<void(const FIntVector&)> SectionChangedCallback;
};
