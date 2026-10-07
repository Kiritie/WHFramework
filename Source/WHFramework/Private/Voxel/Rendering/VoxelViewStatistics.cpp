#include "Voxel/Rendering/VoxelViewManager.h"

#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/VoxelModule.h"

FVoxelTerrainMetricEvidence FVoxelViewManager::SampleTerrainMetric(const FVoxelTerrainMetricEvidence::FTarget& InTarget) const
{
	FVoxelTerrainMetricEvidence Evidence;
	Evidence.Target = InTarget;
	Evidence.Representation = InTarget.Representation;
	if (InTarget.WorldEpoch != WorldEpoch || !Module.GetRuntime()) return Evidence;
	const FVoxelGenerationBounds Bounds{InTarget.Min, InTarget.Max};
	Evidence.NearestDistanceMeters = MinimumObserverDistanceCells(Bounds) * Module.BlockSize() / 100.0;
	const auto& Interest = GetViewInterest();
	const int32 Side = InTarget.Max.X - InTarget.Min.X;
	if (InTarget.Representation >= EVoxelTerrainMetricRepresentation::F0 &&
		InTarget.Representation <= EVoxelTerrainMetricRepresentation::F3Plus)
	{
		const FVoxelFineShardKey Key{InTarget.Min, Side};
		const auto* Slot = FineShardSlots.Find(Key);
		if (Side == 16)
		{
			const auto* Demand = Interest.Exact.Find(Key.Min / 16);
			Evidence.bWanted = Demand && Demand->bFineRender;
			const auto* Data = Module.GetRuntime()->FindSection(Key.Min / 16);
			Evidence.bDataReady = Data && Data->Status == EVoxelSectionStatus::DataReady;
			Evidence.bCollisionReady = Module.IsCollisionReady(Key.Min / 16);
		}
		else
		{
			Evidence.bWanted = Interest.FineShardDemands.Contains(Key);
			Evidence.bDataReady = Slot && Slot->SubmittedStamp.GeometryRevision ==
				Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Key) && Slot->SamplesReady && Slot->SamplesReady->Load();
		}
		Evidence.bMeshReady = Slot && Slot->Build.IsMeshReady() && Slot->Build.GetInput().DataRevision ==
			Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Key) &&
			Module.IsOverlayVersionCurrent(Slot->Build.GetPreparedGeometry()->OverlayVersion);
		Evidence.bDataReady |= Evidence.bMeshReady;
		if (Evidence.bMeshReady)
		{
			const auto Geometry = Slot->Build.GetPreparedGeometry();
			Evidence.Vertices = Geometry->VertexCount;
			Evidence.Triangles = Geometry->TriangleCount;
			Evidence.MeshBytes = Geometry->MeshCapacityBytes;
		}
		const auto Presented = CapturePresentedSnapshot();
		const int32 AtomsPerSide = Side / 16;
		Evidence.OwnershipAtoms = 0;
		int32 LatestAtoms = 0;
		for (int32 Z = 0; Z < AtomsPerSide; ++Z)
			for (int32 Y = 0; Y < AtomsPerSide; ++Y)
				for (int32 X = 0; X < AtomsPerSide; ++X)
				{
					const FIntVector Section = Key.Min / 16 + FIntVector(X, Y, Z);
					if (Section.Z * 16 >= Module.GetManifest().Settings.MaxZ || Section.Z * 16 + 16 <= Module.GetManifest().Settings.MinZ) continue;
					++Evidence.OwnershipAtoms;
					const auto* Actual = Presented ? Presented->Find(Section) : nullptr;
					if (Actual && Actual->Content.WorldEpoch == WorldEpoch && Actual->Content.SampleStepCells == 1 &&
						(Actual->Owner.Kind == EVoxelFineOwnerKind::F0 || Actual->Owner.Kind == EVoxelFineOwnerKind::F1Plus))
						++Evidence.PresentedOwnershipAtoms;
					LatestAtoms += IsLatestFinePresented(Section) ? 1 : 0;
				}
		Evidence.bPresented = Evidence.OwnershipAtoms > 0 && Evidence.PresentedOwnershipAtoms == Evidence.OwnershipAtoms;
		Evidence.bLatestPresented = Evidence.OwnershipAtoms > 0 && LatestAtoms == Evidence.OwnershipAtoms;
	}
	else
	{
		FVoxelViewAdmission Admission;
		const FVoxelTerrainGeometry* Geometry = nullptr;
		AActor* Actor = nullptr;
		uint8 Level = uint8(FMath::FloorLog2(uint32(FMath::Max(16, Side) / 16)));
		if (InTarget.Representation == EVoxelTerrainMetricRepresentation::Proxy)
		{
			Admission.Kind = EVoxelViewAdmissionKind::VoxelProxy;
			Admission.ProxyKey = {InTarget.Min / Side, Level};
			Evidence.bWanted = Interest.VoxelProxy.Contains(Admission.ProxyKey);
			Actor = VoxelProxyActors.FindRef(Admission.ProxyKey);
			Geometry = Interest.FindGeometry(Admission.ProxyKey);
		}
		else if (InTarget.Representation == EVoxelTerrainMetricRepresentation::Surface)
		{
			Admission.Kind = EVoxelViewAdmissionKind::Surface;
			Admission.SurfaceKey = {FIntPoint(InTarget.Min.X / Side, InTarget.Min.Y / Side), Level};
			Evidence.bWanted = Interest.Surface.Contains(Admission.SurfaceKey);
			Actor = SurfaceActors.FindRef(Admission.SurfaceKey);
			Geometry = Interest.FindGeometry(Admission.SurfaceKey);
		}
		else
		{
			Admission.Kind = EVoxelViewAdmissionKind::Macro;
			Admission.MacroKey = {FIntPoint(InTarget.Min.X / Side, InTarget.Min.Y / Side), Level};
			Evidence.bWanted = Interest.Macro.Contains(Admission.MacroKey);
			Actor = MacroActors.FindRef(Admission.MacroKey);
			Geometry = Interest.FindGeometry(Admission.MacroKey);
		}
		Evidence.bDataReady = IsAdmissionDataReady(Admission, false);
		Evidence.bMeshReady = IsAdmissionMeshReady(Admission);
		if (Geometry)
		{
			Evidence.SampleStepCells = Geometry->SampleStepCells;
			if (const auto* Metrics = GeometryMetrics.Find(Geometry->Key); Evidence.bMeshReady && Metrics && Metrics->Geometry == *Geometry)
			{
				Evidence.Vertices = Metrics->Vertices;
				Evidence.Triangles = Metrics->Triangles;
				Evidence.MeshBytes = Metrics->MeshBytes;
			}
		}
		Evidence.bPresented = Actor && Publisher->IsPresented(Actor);
		Evidence.bLatestPresented = IsAdmissionPresented(Admission);
		Evidence.OwnershipAtoms = int64(Side / 16) * (Side / 16) * FMath::Max(1, (InTarget.Max.Z - InTarget.Min.Z) / 16);
		Evidence.PresentedOwnershipAtoms = Evidence.bPresented ? Evidence.OwnershipAtoms : 0;
	}
	if (!Evidence.OwnershipAtoms || (!Evidence.bWanted && !Evidence.PresentedOwnershipAtoms)) Evidence.Target.WorldEpoch = 0;
	return Evidence;
}

void FVoxelViewManager::RefreshTerrainStatistics()
{
	const auto Statistics = Scheduler.GetTerrainStatistics();
	if (!Statistics || !AppliedInterestSnapshot || !AppliedInterestSnapshot->PublicationInputs) return;
	const auto& Inputs = *AppliedInterestSnapshot->PublicationInputs;
	if (StatisticsInputRevision != AppliedInterestSnapshot->PublicationRevision)
	{
		StatisticsInputRevision = AppliedInterestSnapshot->PublicationRevision;
		StatisticsInputPhase = StatisticsInputCursor = 0;
	}
	const double Deadline = TerrainDeadline(0.08);
	for (int32 Work = 0; Work < 64 && FPlatformTime::Seconds() < Deadline; ++Work)
	{
		const double Started = FPlatformTime::Seconds();
		if (!TerrainFrameBudget.TryStart(EVoxelPublishPhase::Priority, Started, 0.01)) return;
		FVoxelTerrainMetricEvidence::FTarget Target;
		Target.WorldEpoch = WorldEpoch;
		if ((Work & 1) != 0)
		{
			Statistics->VisitEvidence(StatisticsEvidenceCursor, [this](const FVoxelTerrainMetricEvidence& Old) { return SampleTerrainMetric(Old.Target); });
		}
		else
		{
			if (StatisticsInputPhase == 0 && Inputs.Fine.IsValidIndex(StatisticsInputCursor))
			{
				Target.Min = Inputs.Fine[StatisticsInputCursor++] * 16;
				Target.Max = Target.Min + FIntVector(16);
				Target.Representation = EVoxelTerrainMetricRepresentation::F0;
			}
			else if (StatisticsInputPhase == 1 && Inputs.FineShards.IsValidIndex(StatisticsInputCursor))
			{
				const auto& Key = Inputs.FineShards[StatisticsInputCursor++];
				Target.Min = Key.Min;
				Target.Max = Key.Bounds().Max;
				Target.Representation = Key.SideCells == 32 ? EVoxelTerrainMetricRepresentation::F1 :
					Key.SideCells == 64 ? EVoxelTerrainMetricRepresentation::F2 : EVoxelTerrainMetricRepresentation::F3Plus;
			}
			else if (StatisticsInputPhase == 2 && Inputs.Geometry.IsValidIndex(StatisticsInputCursor))
			{
				const auto& Geometry = Inputs.Geometry[StatisticsInputCursor++];
				if (!Geometry.Key.TryBounds(Geometry.WorldMinZ, Geometry.WorldMaxZ, Target.Min, Target.Max)) Target.WorldEpoch = 0;
				Target.Representation = Geometry.Key.Representation == EVoxelTerrainRepresentation::VoxelProxy ? EVoxelTerrainMetricRepresentation::Proxy :
					Geometry.Key.Representation == EVoxelTerrainRepresentation::Surface ? EVoxelTerrainMetricRepresentation::Surface : EVoxelTerrainMetricRepresentation::Macro;
			}
			else { StatisticsInputPhase = (StatisticsInputPhase + 1) % 3; StatisticsInputCursor = 0; Target.WorldEpoch = 0; }
			if (Target.IsValid())
			{
				auto Evidence = SampleTerrainMetric(Target);
				if (Evidence.Target.IsValid() && !Statistics->SetEvidence(Evidence)) --StatisticsInputCursor;
			}
		}
		TerrainFrameBudget.Finish(EVoxelPublishPhase::Priority, Started, FPlatformTime::Seconds());
	}
}
