#include "Voxel/Rendering/VoxelFineSafety.h"

#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"

FVoxelFineFeedbackCapture::FVoxelFineFeedbackCapture(TSharedRef<const FVoxelInterestSet, ESPMode::ThreadSafe> InInterest,
	TSharedPtr<const IVoxelTerrainPresentedSnapshot, ESPMode::ThreadSafe> InVisual,
	TSharedPtr<const FVoxelCollisionCoverageSnapshot, ESPMode::ThreadSafe> InPhysics,
	const FVoxelChangeHierarchy& InHierarchy, const FVoxelWorldManifest& InManifest,
	const uint64 InShapeRevision, const uint64 InRootSerial,
	TSharedRef<FVoxelFineResidencyFeedback, ESPMode::ThreadSafe> InFeedback)
	: Interest(InInterest), Visual(MoveTemp(InVisual)), Physics(MoveTemp(InPhysics)), Hierarchy(InHierarchy.CaptureVersionSnapshot()),
	  Manifest(InManifest), ShapeRevision(InShapeRevision), Feedback(InFeedback)
{
	Feedback->WorldEpoch = Visual ? Visual->GetWorldEpoch() : Physics ? Physics->WorldEpoch : 0;
	Feedback->CoverageRootSerial = InRootSerial;
	Feedback->ContentRevision = Hierarchy.ContentRevision;
}

FVoxelFineSectionCoverage FVoxelFineFeedbackCapture::CaptureSection(const FIntVector& InSection)
{
	if (const auto* Existing = Feedback->Sections.Find(InSection)) return *Existing;
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
		Receipt->ContentRevision == Hierarchy.GetSectionRevision(InSection);
	Feedback->Sections.Add(InSection, Coverage);
	return Coverage;
}

bool FVoxelFineFeedbackCapture::Advance(const int32 InMaximumShards, const TAtomic<bool>& InCancel)
{
	if (InCancel.Load() || InMaximumShards < 1) return false;
	if (!Interest->PublicationInputs) return true;
	// 安全查询先完整采集实际运行区；驻留分片只需找到首个缺失项即可判为未就绪。
	const auto& RuntimeSections = Interest->PublicationInputs->RuntimeSections;
	for (int32 Work = 0; Work < InMaximumShards * 8 && RuntimeSections.IsValidIndex(RuntimeCursor); ++Work)
	{
		if (InCancel.Load()) return false;
		CaptureSection(RuntimeSections[RuntimeCursor++]);
	}
	if (RuntimeCursor < RuntimeSections.Num()) return false;
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
