#include "Voxel/Rendering/VoxelFineSafety.h"

#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"

FVoxelFineFeedbackCapture::FVoxelFineFeedbackCapture(TSharedRef<const FVoxelInterestSet, ESPMode::ThreadSafe> InInterest,
	TSharedPtr<const IVoxelTerrainPresentedSnapshot, ESPMode::ThreadSafe> InVisual,
	TSharedPtr<const FVoxelCollisionCoverageSnapshot, ESPMode::ThreadSafe> InPhysics,
	const FVoxelChangeHierarchy& InHierarchy, const FVoxelWorldManifest& InManifest,
	const uint64 InShapeRevision, const uint64 InRootSerial, const bool bInCaptureShards,
	TSharedRef<FVoxelFineResidencyFeedback, ESPMode::ThreadSafe> InFeedback)
	: Interest(InInterest), Visual(MoveTemp(InVisual)), Physics(MoveTemp(InPhysics)), Hierarchy(InHierarchy.CaptureVersionSnapshot()),
	  Manifest(InManifest), ShapeRevision(InShapeRevision), bCaptureShards(bInCaptureShards), Feedback(InFeedback)
{
	Feedback->WorldEpoch = Visual ? Visual->GetWorldEpoch() : Physics ? Physics->WorldEpoch : 0;
	Feedback->CoverageRootSerial = InRootSerial;
	Feedback->ContentRevision = Hierarchy.ContentRevision;
}

FVoxelFineSectionCoverage FVoxelFineFeedbackCapture::BuildSectionCoverage(const FIntVector& InSection) const
{
	FVoxelFineSectionCoverage Coverage;
	Coverage.WorldEpoch = Feedback->WorldEpoch;
	Coverage.CoverageRootSerial = Feedback->CoverageRootSerial;
	if (const auto* Evidence = Visual ? Visual->Find(InSection) : nullptr;
		Evidence && (Evidence->Owner.Kind == EVoxelFineOwnerKind::F0 || Evidence->Owner.Kind == EVoxelFineOwnerKind::F1Plus))
	{
		const auto Geometry = StaticCastSharedPtr<const FVoxelFineShardGeometry>(Evidence->GeometryLease);
		Coverage.bWholeVisualCommitted = Evidence->Content.WorldEpoch == Feedback->WorldEpoch && Geometry &&
			Evidence->Content.SampleStepCells == 1 && Evidence->bGeometryReady;
		Coverage.bContentVersionsCurrent = Coverage.bWholeVisualCommitted &&
			Evidence->Content.GeneratorIdentity == Manifest.BaseSampleHash && Evidence->Content.RecipeRevision == Manifest.RecipeHash &&
			Evidence->Content.RegistryRevision == Manifest.RegistryHash && Evidence->Content.ShapeRevision == ShapeRevision &&
			Evidence->Content.DataRevision == Geometry->Attempt.Input.DataRevision &&
			Evidence->Content.OverlayRevision == Geometry->Attempt.Input.OverlayRevision &&
			Geometry->Attempt.Input.DataRevision == Hierarchy.GetFineShardRevision(Geometry->Attempt.Input.Shard);
	}
	const auto* Receipt = Physics ? Physics->Find(InSection) : nullptr;
	Coverage.bWholeCollisionCommitted = Receipt && Receipt->WorldEpoch == Feedback->WorldEpoch &&
		Receipt->PhysicsStep != INDEX_NONE && Receipt->BodyGeneration != 0 &&
		Receipt->RegistryRevision == Manifest.RegistryHash && Receipt->ShapeRevision == ShapeRevision &&
		Receipt->ContentRevision == Hierarchy.GetSectionRevision(InSection) &&
		Receipt->NeighborRevision == Hierarchy.GetNeighborSectionRevision(InSection);
	return Coverage;
}

FVoxelFineSectionCoverage FVoxelFineFeedbackCapture::CaptureSection(const FIntVector& InSection)
{
	if (const auto* Existing = Feedback->Sections.Find(InSection)) return *Existing;
	const FVoxelFineSectionCoverage Coverage = BuildSectionCoverage(InSection);
	Feedback->Sections.Add(InSection, Coverage);
	return Coverage;
}

FVoxelFineSectionCoverage FVoxelFineFeedbackCapture::ReadSection(const FIntVector& InSection) const
{
	if (const auto* Existing = Feedback->Sections.Find(InSection)) return *Existing;
	// 与原先预采集的运行区一致：非运行需求且未被分片检查过的 Section 仍视为缺失。
	return Interest->Exact.Contains(InSection) ? BuildSectionCoverage(InSection) : FVoxelFineSectionCoverage();
}

bool FVoxelFineFeedbackCapture::Advance(const int32 InMaximumShards, const TAtomic<bool>& InCancel)
{
	if (InCancel.Load() || InMaximumShards < 1) return false;
	// 没有待交接窗口时，安全范围由 ReadSection 按需查询，不预扫整个 Fine 驻留区。
	if (!bCaptureShards || !Interest->PublicationInputs) return true;
	// 安全扫掠按需从冻结的视觉和物理回执根读取；分片检查仍保留有界续跑。
	const auto& Shards = Interest->PublicationInputs->FineShards;
	for (int32 Work = 0; Work < InMaximumShards && Shards.IsValidIndex(ShardCursor); ++Work)
	{
		if (InCancel.Load()) return false;
		const auto& Key = Shards[ShardCursor++];
		const int32 Side = Key.SideCells / 16;
		bool bCurrent = true;
		for (int32 Z = 0; Z < Side && bCurrent; ++Z)
			for (int32 Y = 0; Y < Side && bCurrent; ++Y)
				for (int32 X = 0; X < Side && bCurrent; ++X)
				{
					const auto Coverage = CaptureSection(Key.Min / 16 + FIntVector(X, Y, Z));
					bCurrent &= Coverage.bWholeVisualCommitted && Coverage.bContentVersionsCurrent;
				}
		Feedback->Shards.Add(Key, {bCurrent, bCurrent, bCurrent});
	}
	return ShardCursor >= Shards.Num();
}

TSharedRef<const FVoxelFineResidencyFeedback, ESPMode::ThreadSafe> FVoxelFineFeedbackCapture::GetFeedback() const
{
	return Feedback;
}

uint64 FVoxelFineFeedbackCapture::GetRenderSerial() const
{
	return Visual ? Visual->GetSerial() : 0;
}

uint64 FVoxelFineFeedbackCapture::GetPhysicsSerial() const
{
	return Physics ? Physics->Serial : 0;
}
