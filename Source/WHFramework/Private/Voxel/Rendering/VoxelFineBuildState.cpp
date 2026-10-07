#include "Voxel/Rendering/VoxelFineBuildState.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"

bool FVoxelFineBuildState::Initialize(const FVoxelFineMeshInput& InInput, FString& OutError)
{
	if (InInput.WorldEpoch == 0 || InInput.Incarnation == 0 || InInput.GeneratorIdentity == 0 ||
		!InInput.Shard.Validate(OutError))
	{
		OutError = TEXT("Fine 构建身份不合法");
		return false;
	}
	if (Input.Incarnation != 0)
	{
		OutError = TEXT("已使用的构建状态不能复用；新驻留实例必须分配新 Incarnation");
		return false;
	}
	Input = InInput;
	OutError.Reset();
	return true;
}

FVoxelFineBuildAttempt FVoxelFineBuildState::BeginAttempt()
{
	check(Input.WorldEpoch != 0 && Input.Incarnation != 0);
	if (Pending.IsSet() && Pending.GetValue().Input == Input)
	{
		return Pending.GetValue();
	}
	check(NextAttempt < MAX_uint64);
	const FVoxelFineBuildAttempt Attempt{++NextAttempt, Input};
	Pending = Attempt;
	Failure.Reset();
	return Attempt;
}

EVoxelFineAttemptDisposition FVoxelFineBuildState::Complete(const FVoxelFineBuildAttempt& InAttempt, const bool bInCanceled,
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> InGeometry, FString InError)
{
	if (!Pending.IsSet() || Pending.GetValue() != InAttempt || !IsCurrent(InAttempt))
	{
		return EVoxelFineAttemptDisposition::Stale;
	}
	Pending.Reset();
	if (bInCanceled)
	{
		return EVoxelFineAttemptDisposition::Canceled;
	}
	const bool bValid = InGeometry && InGeometry->Attempt == InAttempt && InGeometry->bCoverageValidated &&
		!InGeometry->Pieces.IsEmpty() &&
		InGeometry->Boundary && InGeometry->Boundary->Input == InAttempt.Input && InGeometry->Boundary->Validate();
	if (!InError.IsEmpty() || !bValid)
	{
		Failure = InError.IsEmpty() ? TEXT("Fine 几何包缺少完整覆盖或空网格证明") : MoveTemp(InError);
		return EVoxelFineAttemptDisposition::Failed;
	}
	Prepared = InAttempt;
	PreparedGeometry = MoveTemp(InGeometry);
	bPreparedEmpty = PreparedGeometry->bKnownEmpty;
	Failure.Reset();
	return EVoxelFineAttemptDisposition::Accepted;
}

void FVoxelFineBuildState::InvalidatePrepared()
{
	Pending.Reset();
	Prepared.Reset();
	PreparedGeometry.Reset();
	bPreparedEmpty = false;
	Failure.Reset();
}

bool FVoxelFineBuildState::NotifyContentChanged(const uint64 InGeneratorIdentity, const uint64 InRegistryRevision,
	const uint64 InShapeRevision, const uint64 InDataRevision, const uint64 InOverlayRevision)
{
	check(InGeneratorIdentity != 0);
	check(InDataRevision >= Input.DataRevision && InOverlayRevision >= Input.OverlayRevision);
	if (InGeneratorIdentity == Input.GeneratorIdentity && InRegistryRevision == Input.RegistryRevision &&
		InShapeRevision == Input.ShapeRevision && InDataRevision == Input.DataRevision && InOverlayRevision == Input.OverlayRevision)
	{
		return false;
	}
	Input.GeneratorIdentity = InGeneratorIdentity;
	Input.RegistryRevision = InRegistryRevision;
	Input.ShapeRevision = InShapeRevision;
	Input.DataRevision = InDataRevision;
	Input.OverlayRevision = InOverlayRevision;
	check(Input.MeshRevision < MAX_uint64);
	++Input.MeshRevision;
	InvalidatePrepared();
	return true;
}

void FVoxelFineBuildState::NotifyDataChanged(const uint64 InDataRevision, const uint64 InOverlayRevision)
{
	NotifyContentChanged(Input.GeneratorIdentity, Input.RegistryRevision, Input.ShapeRevision, InDataRevision, InOverlayRevision);
}

void FVoxelFineBuildState::NotifyMeshPolicyChanged(const uint64 InRegistryRevision, const uint64 InShapeRevision)
{
	NotifyContentChanged(Input.GeneratorIdentity, InRegistryRevision, InShapeRevision, Input.DataRevision, Input.OverlayRevision);
}

void FVoxelFineBuildState::NotifyTopologyChanged()
{
	check(TopologyRevision < MAX_uint64);
	++TopologyRevision;
}

bool FVoxelFineBuildState::IsCurrent(const FVoxelFineBuildAttempt& InAttempt) const
{
	return InAttempt.Serial != 0 && InAttempt.Input == Input;
}

bool FVoxelFineBuildState::IsMeshReady() const
{
	return Prepared.IsSet() && IsCurrent(Prepared.GetValue()) && PreparedGeometry.IsValid();
}

bool FVoxelFineBuildState::IsKnownEmpty() const
{
	return IsMeshReady() && bPreparedEmpty;
}

const FVoxelFineMeshInput& FVoxelFineBuildState::GetInput() const
{
	return Input;
}

const TOptional<FVoxelFineBuildAttempt>& FVoxelFineBuildState::GetPendingAttempt() const
{
	return Pending;
}

const TOptional<FVoxelFineBuildAttempt>& FVoxelFineBuildState::GetPreparedAttempt() const
{
	return Prepared;
}

TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FVoxelFineBuildState::GetPreparedGeometry() const
{
	return PreparedGeometry;
}

const FString& FVoxelFineBuildState::GetFailure() const
{
	return Failure;
}

uint64 FVoxelFineBuildState::GetTopologyRevision() const
{
	return TopologyRevision;
}
