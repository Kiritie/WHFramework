#include "Voxel/Generation/VoxelStructure.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelStructurePlan.h"

namespace
{
	bool CheckStructureAllocation(
		FString& OutError,
		FVoxelGenerationWorkspace* InWorkspace = nullptr)
	{
		if (InWorkspace)
		{
			return InWorkspace->Check(OutError);
		}
		const auto* Allocation = FVoxelGenerationAllocationScope::Get();
		return Allocation ? Allocation->Check(OutError) :
			FVoxelGenerationAllocationScope::AllowPassive(OutError);
	}

	bool FailStructureAllocation(
		const TCHAR* InError,
		FString& OutError,
		FVoxelGenerationWorkspace* InWorkspace = nullptr)
	{
		if (InWorkspace)
		{
			return InWorkspace->Fail(InError, OutError);
		}
		if (auto* Allocation = FVoxelGenerationAllocationScope::Get())
		{
			return Allocation->Fail(InError, OutError);
		}
		OutError = InError;
		return false;
	}

	bool CheckStructureCancel(
		const TAtomic<bool>* InCancel,
		FString& OutError)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		return true;
	}

	template<typename ElementType>
	bool ReserveStructureArray(
		TArray<ElementType>& InArray,
		const int32 InCapacity,
		FString& OutError,
		FVoxelGenerationWorkspace* InWorkspace)
	{
		return InWorkspace ? InWorkspace->ReserveArray(InArray, InCapacity, OutError) :
			VoxelGenerationAllocation::RetainArray(InArray, InCapacity, OutError);
	}

	template<typename ElementType>
	bool ReserveStructureAppend(
		TArray<ElementType>& InArray,
		FString& OutError,
		FVoxelGenerationWorkspace* InWorkspace)
	{
		if (InArray.Num() == MAX_int32)
		{
			return FailStructureAllocation(TEXT("Oversized: structure array count exceeds native capacity"),
				OutError, InWorkspace);
		}
		const int32 Required = InArray.Num() + 1;
		const int32 Capacity = Required <= InArray.Max() ? Required :
			int32(FMath::Min<int64>(MAX_int32,
				FMath::Max<int64>(Required, FMath::Max<int64>(4, int64(InArray.Max()) * 2))));
		return ReserveStructureArray(InArray, Capacity, OutError, InWorkspace);
	}

	template<typename ElementType>
	void FreeStructureArray(
		TArray<ElementType>& InArray,
		FVoxelGenerationWorkspace* InWorkspace)
	{
		if (InWorkspace)
		{
			InWorkspace->FreeArray(InArray);
		}
		else
		{
			VoxelGenerationAllocation::FreeRetainedArray(InArray);
		}
	}

	void FreeStructureInstance(
		FVoxelStructureInstance& InInstance,
		FVoxelGenerationWorkspace* InWorkspace)
	{
		FreeStructureArray(InInstance.Pieces, InWorkspace);
		FreeStructureArray(InInstance.Writes, InWorkspace);
		FreeStructureArray(InInstance.ClearVolumes, InWorkspace);
		FreeStructureArray(InInstance.Details, InWorkspace);
	}

	void FreeStructureInstances(
		TArray<FVoxelStructureInstance>& InInstances,
		FVoxelGenerationWorkspace* InWorkspace)
	{
		for (FVoxelStructureInstance& Instance : InInstances)
		{
			FreeStructureInstance(Instance, InWorkspace);
		}
		FreeStructureArray(InInstances, InWorkspace);
	}

	FInt64Vector RotateCell(
		FInt64Vector InPosition,
		uint8 InYaw)
	{
		for (uint8 Turn = 0;
			Turn < (InYaw & 3);
			++Turn)
		{
			InPosition =
				FInt64Vector(
					-InPosition.Y - 1,
					InPosition.X,
					InPosition.Z);
		}

		return InPosition;
	}

	FInt64Vector RotateCorner(
		FInt64Vector InPosition,
		uint8 InYaw)
	{
		for (uint8 Turn = 0;
			Turn < (InYaw & 3);
			++Turn)
		{
			InPosition =
				FInt64Vector(
					-InPosition.Y,
					InPosition.X,
					InPosition.Z);
		}

		return InPosition;
	}

	bool TransformBounds(
		const FVoxelGenerationBounds& InBounds,
		const FInt64Vector& InOrigin,
		uint8 InYaw,
		FVoxelGenerationBounds& OutBounds)
	{
		if (!InBounds.IsValid())
		{
			return false;
		}
		FInt64Vector Min(MAX_int64, MAX_int64, MAX_int64);
		FInt64Vector Max(MIN_int64, MIN_int64, MIN_int64);

		for (int32 CornerIndex = 0;
			CornerIndex < 8;
			++CornerIndex)
		{
			const FInt64Vector Corner(
				(CornerIndex & 1)
					? InBounds.Max.X
					: InBounds.Min.X,
				(CornerIndex & 2)
					? InBounds.Max.Y
					: InBounds.Min.Y,
				(CornerIndex & 4)
					? InBounds.Max.Z
					: InBounds.Min.Z);

			const FInt64Vector Transformed =
				RotateCorner(
					Corner,
					InYaw) +
				InOrigin;

			Min.X =
				FMath::Min(
					Min.X,
					Transformed.X);

			Min.Y =
				FMath::Min(
					Min.Y,
					Transformed.Y);

			Min.Z =
				FMath::Min(
					Min.Z,
					Transformed.Z);

			Max.X =
				FMath::Max(
					Max.X,
					Transformed.X);

			Max.Y =
				FMath::Max(
					Max.Y,
					Transformed.Y);

			Max.Z =
				FMath::Max(
					Max.Z,
					Transformed.Z);
		}

		return VoxelGeneration::ClipWorldBounds(Min, Max, OutBounds);
	}
}

uint64 FVoxelStructureInstance::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Pieces) +
		VoxelGenerationAllocation::AllocatedBytes(Writes) +
		VoxelGenerationAllocation::AllocatedBytes(ClearVolumes) +
		VoxelGenerationAllocation::AllocatedBytes(Details);
}

bool FVoxelStructureLayoutContext::ReservePieces(
	TArray<FVoxelStructurePiecePlacement>& InOutPieces,
	const int32 InCapacity,
	FString& OutError) const
{
	if (!Workspace)
	{
		return FailStructureAllocation(TEXT("InvalidAllocationScope: structure layout requires its caller workspace"),
			OutError);
	}
	return CheckStructureCancel(Cancel, OutError) &&
		Workspace->ReserveArray(InOutPieces, InCapacity, OutError);
}

FVoxelStructureLayoutRegistry&
FVoxelStructureLayoutRegistry::Get()
{
	static FVoxelStructureLayoutRegistry Instance;
	return Instance;
}

bool FVoxelStructureLayoutRegistry::Register(
	TSharedRef<
		const IVoxelStructureLayoutAlgorithm,
		ESPMode::ThreadSafe> InAlgorithm,
	FString& OutError)
{
	const FName AlgorithmId =
		InAlgorithm->GetId();

	if (AlgorithmId.IsNone())
	{
		OutError = TEXT("Voxel structure layout algorithm id cannot be None");
		return false;
	}

	if (InAlgorithm->GetVersion() == 0)
	{
		OutError = FString::Printf(
			TEXT("Voxel structure layout algorithm %s has version zero"),
			*AlgorithmId.ToString());

		return false;
	}

	FWriteScopeLock ScopeLock(Lock);

	if (Algorithms.Contains(AlgorithmId))
	{
		OutError = FString::Printf(
			TEXT("Voxel structure layout algorithm is already registered: %s"),
			*AlgorithmId.ToString());

		return false;
	}

	Algorithms.Add(
		AlgorithmId,
		MoveTemp(InAlgorithm));

	OutError.Reset();
	return true;
}

void FVoxelStructureLayoutRegistry::Unregister(
	FName InAlgorithmId)
{
	FWriteScopeLock ScopeLock(Lock);
	Algorithms.Remove(InAlgorithmId);
}

TSharedPtr<
	const IVoxelStructureLayoutAlgorithm,
	ESPMode::ThreadSafe>
FVoxelStructureLayoutRegistry::Find(
	FName InAlgorithmId) const
{
	FReadScopeLock ScopeLock(Lock);

	const TSharedRef<
		const IVoxelStructureLayoutAlgorithm,
		ESPMode::ThreadSafe>* Found =
			Algorithms.Find(
				InAlgorithmId);

	return Found
		? TSharedPtr<
			const IVoxelStructureLayoutAlgorithm,
			ESPMode::ThreadSafe>(*Found)
		: nullptr;
}

void FVoxelStructureLayoutRegistry::Reset()
{
	FWriteScopeLock ScopeLock(Lock);
	Algorithms.Reset();
}

FVoxelStructurePlanner::FVoxelStructurePlanner(
	TSharedRef<
		const FVoxelGenerationRecipe,
		ESPMode::ThreadSafe> InRecipe)
	: Recipe(InRecipe)
{
}

bool FVoxelStructurePlanner::BuildPlan(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn)> InSampleColumn,
	FVoxelStructurePlan& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!CheckStructureAllocation(OutError))
	{
		return false;
	}
	FVoxelStructurePlan BuiltPlan;
	ON_SCOPE_EXIT
	{
		const uint64 Bytes = BuiltPlan.GetAllocatedBytes();
		BuiltPlan = FVoxelStructurePlan();
		if (auto* Allocation = FVoxelGenerationAllocationScope::Get())
		{
			Allocation->ReleaseRetainedBytes(Bytes);
		}
	};
	FVoxelGenerationWorkspace Workspace;
	TArray<FVoxelStructureInstance> Instances;
	ON_SCOPE_EXIT
	{
		FreeStructureInstances(Instances, &Workspace);
	};
	if (!Plan(InBounds, InSampleColumn, Instances, OutError, InCancel, &Workspace))
	{
		return false;
	}
	BuiltPlan.Bounds = InBounds;
	int64 ClearCount = 0;
	int64 WriteCount = 0;
	int64 DetailCount = 0;
	for (const FVoxelStructureInstance& Instance : Instances)
	{
		if (!CheckStructureCancel(InCancel, OutError))
		{
			return false;
		}
		const int32 DefinitionIndex = Recipe->FindStructure(Instance.DefinitionId);
		if (!Recipe->Structures.IsValidIndex(DefinitionIndex))
		{
			continue;
		}
		ClearCount += Instance.ClearVolumes.Num();
		WriteCount += Instance.Writes.Num();
		DetailCount += Instance.Details.Num();
		if (ClearCount > MAX_int32 || WriteCount > MAX_int32 || DetailCount > MAX_int32)
		{
			return FailStructureAllocation(TEXT("Oversized: structure plan count exceeds native capacity"), OutError);
		}
	}
	if (!VoxelGenerationAllocation::RetainArray(BuiltPlan.Clears, int32(ClearCount), OutError) ||
		!VoxelGenerationAllocation::RetainArray(BuiltPlan.Writes, int32(WriteCount), OutError) ||
		!VoxelGenerationAllocation::RetainArray(BuiltPlan.Details, int32(DetailCount), OutError))
	{
		return false;
	}
	for (const FVoxelStructureInstance& Instance : Instances)
	{
		if (!CheckStructureCancel(InCancel, OutError))
		{
			return false;
		}
		const int32 DefinitionIndex = Recipe->FindStructure(Instance.DefinitionId);
		if (!Recipe->Structures.IsValidIndex(DefinitionIndex))
		{
			continue;
		}
		const EVoxelGenerationStage Stage = Recipe->Structures[DefinitionIndex].Stage;
		for (const FVoxelStructureClearVolume& Clear : Instance.ClearVolumes)
		{
			if (!CheckStructureCancel(InCancel, OutError))
			{
				return false;
			}
			BuiltPlan.Clears.Add({Clear.Bounds, Stage, Instance.Id});
		}
		for (const FVoxelStructureCellWrite& Write : Instance.Writes)
		{
			if (!CheckStructureCancel(InCancel, OutError))
			{
				return false;
			}
			BuiltPlan.Writes.Add({Write.Position, Write.Value, Stage, Instance.Id});
		}
		BuiltPlan.Details.Append(Instance.Details);
	}
	// 两份 backing 的复制峰值已授予；正文索引重建前先释放临时 instances。
	FreeStructureInstances(Instances, &Workspace);
	if (!BuiltPlan.Finalize(OutError, InCancel))
	{
		return false;
	}
	const uint64 OldBytes = OutPlan.GetAllocatedBytes();
	OutPlan = MoveTemp(BuiltPlan);
	if (auto* Allocation = FVoxelGenerationAllocationScope::Get())
	{
		Allocation->ReleaseRetainedBytes(OldBytes);
	}
	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::Plan(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn)> InSampleColumn,
	TArray<FVoxelStructureInstance>& OutInstances,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	FVoxelGenerationWorkspace* InOutputWorkspace) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_StructurePlan);

	if (!CheckStructureAllocation(OutError, InOutputWorkspace) ||
		!CheckStructureCancel(InCancel, OutError))
	{
		return false;
	}
	if (!InBounds.IsValid())
	{
		OutError = TEXT("Voxel structure planner received invalid bounds");
		return false;
	}

	TArray<FVoxelStructureInstance> Instances;
	ON_SCOPE_EXIT
	{
		FreeStructureInstances(Instances, InOutputWorkspace);
	};

	for (int32 DefinitionIndex = 0;
		DefinitionIndex < Recipe->Structures.Num();
		++DefinitionIndex)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}

		const FVoxelStructureRuntimeDefinition& Definition =
			Recipe->Structures[DefinitionIndex];

		FVoxelGenerationWorkspace Workspace;
		TArray<FIntVector> Candidates;

		if (!GatherCandidates(
			Definition,
			InBounds,
			Candidates,
			OutError,
			Workspace,
			InCancel))
		{
			return false;
		}

		for (const FIntVector& Candidate :
			Candidates)
		{
			if (InCancel &&
				InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}

			FVoxelColumnSample Column;

			if (!InSampleColumn(
				Candidate,
				Column))
			{
				continue;
			}

			if (!Recipe->Biomes.IsValidIndex(
				Column.BiomeIndex))
			{
				continue;
			}

			const FVoxelBiomeRuntimeDefinition& Biome =
				Recipe->Biomes[
					Column.BiomeIndex];

			if (!Biome.StructureIndices.Contains(
				DefinitionIndex))
			{
				continue;
			}

			if (Column.SlopePermille >
				Definition.Placement.MaxSlopePermille)
			{
				continue;
			}

			const int64 CandidateZ =
				Definition.Placement.bRequireSurface
					? int64(Column.SurfaceZ) + 1
					: Candidate.Z;

			if (!VoxelGeneration::IsWorldCell(CandidateZ) || CandidateZ <
					Definition.Placement.MinZ ||
				CandidateZ >
					Definition.Placement.MaxZ)
			{
				continue;
			}

			FIntVector Anchor;
			if (!ResolveTerrainAdaptedAnchor(
					Definition,
					FIntVector(
						Candidate.X,
						Candidate.Y,
						int32(CandidateZ)),
					Column, Anchor))
			{
				continue;
			}

			const uint64 CandidateSeed =
				VoxelGeneration::MakeSeed(
					Recipe->Settings.Seed,
					Anchor,
					Definition.StableHash);

			if (VoxelGeneration::RandomRange(
				CandidateSeed,
				0,
				999) >=
				Definition.Placement.ChancePermille)
			{
				continue;
			}

			FVoxelStructureInstance Instance;
			ON_SCOPE_EXIT
			{
				FreeStructureInstance(Instance, InOutputWorkspace);
			};

			if (!BuildInstance(
				DefinitionIndex,
					Definition,
					Anchor,
					Instance,
				OutError,
				{},
				{},
				InOutputWorkspace,
				InCancel))
			{
				return false;
			}

			bool bTouchesQuery =
				false;

			for (const FVoxelStructureCellWrite& Write :
				Instance.Writes)
			{
				if (InBounds.Contains(
					Write.Position))
				{
					bTouchesQuery =
						true;

					break;
				}
			}

			if (!bTouchesQuery)
			{
				for (const FVoxelStructureClearVolume& ClearVolume :
					Instance.ClearVolumes)
				{
					if (ClearVolume.Bounds.Intersects(
						InBounds))
					{
						bTouchesQuery =
							true;

						break;
					}
				}
			}

			if (bTouchesQuery)
			{
				if (!ReserveStructureAppend(Instances, OutError, InOutputWorkspace))
				{
					return false;
				}
				Instances.Add(
					MoveTemp(Instance));
			}
		}
	}

	Instances.Sort(
		[](const FVoxelStructureInstance& InA,
		   const FVoxelStructureInstance& InB)
		{
			return InA.Id < InB.Id;
		});

	if (!CheckStructureCancel(InCancel, OutError))
	{
		return false;
	}
	FreeStructureInstances(OutInstances, InOutputWorkspace);
	OutInstances =
		MoveTemp(Instances);

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::BuildAt(
	const FVoxelPlannedStructurePlacement& InPlacement,
	FVoxelStructureInstance& OutInstance,
	FString& OutError) const
{
	if (!CheckStructureAllocation(OutError))
	{
		return false;
	}
	if (!InPlacement.IsValid())
	{
		OutError = TEXT("Planned voxel structure placement is invalid");
		return false;
	}
	const int32 DefinitionIndex = Recipe->FindStructure(InPlacement.DefinitionId);
	if (!Recipe->Structures.IsValidIndex(DefinitionIndex))
	{
		OutError = FString::Printf(TEXT("Unknown planned voxel structure definition: %s"),
			*InPlacement.DefinitionId.ToString());
		return false;
	}
	return BuildInstance(
		DefinitionIndex,
		Recipe->Structures[DefinitionIndex],
		InPlacement.Anchor,
		OutInstance,
		OutError,
		InPlacement.Id,
		InPlacement.Yaw);
}

bool FVoxelStructurePlanner::GatherCandidates(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutCandidates,
	FString& OutError,
	FVoxelGenerationWorkspace& InWorkspace,
	const TAtomic<bool>* InCancel) const
{
	if (!InWorkspace.Check(OutError))
	{
		return false;
	}

	const int32 Spacing =
		FMath::Max(
			1,
			InDefinition.Placement.Spacing);

	const int32 Separation =
		FMath::Clamp(
			InDefinition.Placement.Separation,
			0,
			FMath::Max(
				0,
				Spacing / 2 - 1));

	const int64 MinGridX =
		VoxelGeneration::FloorDivideWide(
			InBounds.Min.X,
			Spacing) - 1;

	const int64 MaxGridX =
		VoxelGeneration::FloorDivideWide(
			InBounds.Max.X - 1,
			Spacing);

	const int64 MinGridY =
		VoxelGeneration::FloorDivideWide(
			InBounds.Min.Y,
			Spacing) - 1;

	const int64 MaxGridY =
		VoxelGeneration::FloorDivideWide(
			InBounds.Max.Y - 1,
			Spacing);

	const auto VisitCandidates = [&](TFunctionRef<bool(const FIntVector&)> Visit)
	{
		for (int64 GridY = MinGridY;
			GridY <= MaxGridY;
			++GridY)
		{
			if (!CheckStructureCancel(InCancel, OutError))
			{
				return false;
			}
			for (int64 GridX = MinGridX;
				GridX <= MaxGridX;
				++GridX)
			{
				if (!CheckStructureCancel(InCancel, OutError))
				{
					return false;
				}
				if (GridX < MIN_int32 || GridX > MAX_int32 || GridY < MIN_int32 || GridY > MAX_int32)
				{
					continue;
				}
				const FIntVector Grid(
					int32(GridX),
					int32(GridY),
					0);

				const uint64 Seed =
					VoxelGeneration::MakeSeed(
						Recipe->Settings.Seed,
						Grid,
						InDefinition.StableHash);

				const int32 MinOffset =
					Separation;

				const int32 MaxOffset =
					FMath::Max(
						MinOffset,
						Spacing -
						Separation -
						1);

				const int32 OffsetX =
					VoxelGeneration::RandomRange(
						VoxelGeneration::Mix(
							Seed ^ 0x62AB913E00F49217ull),
						MinOffset,
						MaxOffset);

				const int32 OffsetY =
					VoxelGeneration::RandomRange(
						VoxelGeneration::Mix(
							Seed ^ 0xE2F20D0948C3A115ull),
						MinOffset,
						MaxOffset);

				FIntVector Candidate;
				if (!VoxelGeneration::TryWorldCell(FInt64Vector(GridX * Spacing + OffsetX,
					GridY * Spacing + OffsetY, 0), Candidate))
				{
					continue;
				}

				if (Candidate.X < InBounds.Min.X ||
					Candidate.Y < InBounds.Min.Y ||
					Candidate.X >= InBounds.Max.X ||
					Candidate.Y >= InBounds.Max.Y)
				{
					continue;
				}

				if (!Visit(Candidate))
				{
					return false;
				}
			}
		}
		return true;
	};
	int64 Count = 0;
	if (!VisitCandidates([&](const FIntVector&)
	{
		if (Count == MAX_int32)
		{
			return InWorkspace.Fail(TEXT("Oversized: structure candidate count exceeds native capacity"), OutError);
		}
		++Count;
		return true;
	}) || !InWorkspace.ReserveArray(OutCandidates, int32(Count), OutError))
	{
		return false;
	}
	OutCandidates.Reset();
	return VisitCandidates([&](const FIntVector& Candidate)
	{
		OutCandidates.Add(Candidate);
		return true;
	});
}

bool FVoxelStructurePlanner::BuildInstance(
	int32 InDefinitionIndex,
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FIntVector& InCandidate,
	FVoxelStructureInstance& OutInstance,
	FString& OutError,
	TOptional<FVoxelStableId> InId,
	TOptional<uint8> InRootYaw,
	FVoxelGenerationWorkspace* InOutputWorkspace,
	const TAtomic<bool>* InCancel) const
{
	if (!CheckStructureAllocation(OutError, InOutputWorkspace) ||
		!CheckStructureCancel(InCancel, OutError))
	{
		return false;
	}
	if (!VoxelGeneration::IsWorldCell(InCandidate.X) || !VoxelGeneration::IsWorldCell(InCandidate.Y) ||
		!VoxelGeneration::IsWorldCell(InCandidate.Z))
	{
		OutError = TEXT("CoordinateOutOfRange: voxel structure anchor");
		return false;
	}
	const uint64 CandidateSeed =
		VoxelGeneration::MakeSeed(
			Recipe->Settings.Seed,
			InCandidate,
			InDefinition.StableHash);

	FVoxelGenerationWorkspace Workspace;
	TArray<FVoxelStructurePiecePlacement> Pieces;

	if (!BuildPiecePlacements(
		InDefinition,
		InCandidate,
		CandidateSeed,
		Pieces,
		OutError,
		Workspace,
		InCancel))
	{
		return false;
	}

	// 超过全部 int32 模板与世界坐标可抵消范围的偏移不可能产生合法 cell。
	constexpr int64 MaximumLocalOffset = 6ll * (int64(MAX_int32) + 1);
	for (const FVoxelStructurePiecePlacement& Placement : Pieces)
	{
		if (Placement.Origin.X < -MaximumLocalOffset || Placement.Origin.X > MaximumLocalOffset ||
			Placement.Origin.Y < -MaximumLocalOffset || Placement.Origin.Y > MaximumLocalOffset ||
			Placement.Origin.Z < -MaximumLocalOffset || Placement.Origin.Z > MaximumLocalOffset)
		{
			OutError = TEXT("CoordinateOutOfRange: voxel structure local origin");
			return false;
		}
	}

	if (InRootYaw.IsSet())
	{
		for (FVoxelStructurePiecePlacement& Placement : Pieces)
		{
			if (InDefinition.LayoutAlgorithmId.IsNone())
			{
				Placement.Yaw = InRootYaw.GetValue();
			}
			else
			{
				Placement.Origin = RotateCorner(Placement.Origin, InRootYaw.GetValue());
				Placement.Yaw = (Placement.Yaw + InRootYaw.GetValue()) & 3;
			}
		}
	}

	FVoxelStructureInstance Instance;
	ON_SCOPE_EXIT
	{
		FreeStructureInstance(Instance, InOutputWorkspace);
	};

	Instance.Id = InId.IsSet()
		? InId.GetValue()
		: VoxelGeneration::MakeStableId(
			Recipe->Settings.Seed,
			InCandidate,
			InDefinition.StableHash,
			static_cast<uint64>(
				InDefinitionIndex));

	Instance.DefinitionId =
		InDefinition.StableId;

	Instance.Anchor =
		InCandidate;

	if (!ReserveStructureArray(Instance.Pieces, Pieces.Num(), OutError, InOutputWorkspace))
	{
		return false;
	}
	Instance.Pieces.Append(Pieces);
	Workspace.FreeArray(Pieces);

	for (const FVoxelStructurePiecePlacement& Placement :
		Instance.Pieces)
	{
		if (!CheckStructureCancel(InCancel, OutError))
		{
			return false;
		}
		if (!RasterizePiece(
			InDefinition,
			Placement,
			InCandidate,
			Instance.Writes,
			Instance.ClearVolumes,
			OutError,
			InOutputWorkspace,
			InCancel))
		{
			return false;
		}

		const FVoxelStructurePieceTemplate& Piece =
			InDefinition.Pieces[Placement.PieceIndex];
		const FInt64Vector PieceOrigin =
			FInt64Vector(InCandidate) + Placement.Origin - RotateCorner(FInt64Vector(Piece.Entrance), Placement.Yaw);
		int64 DetailCount = Instance.Details.Num();
		for (const FVoxelStructureDetailRuntimeSocket& Socket : Piece.Details)
		{
			if (!CheckStructureCancel(InCancel, OutError))
			{
				return false;
			}
			FIntVector Position;
			if (VoxelGeneration::TryWorldCell(PieceOrigin + RotateCorner(FInt64Vector(Socket.SourceCorner), Placement.Yaw), Position))
			{
				if (++DetailCount > MAX_int32)
				{
					return FailStructureAllocation(TEXT("Oversized: structure detail count exceeds native capacity"),
						OutError, InOutputWorkspace);
				}
			}
		}
		if (!ReserveStructureArray(Instance.Details, int32(DetailCount), OutError, InOutputWorkspace))
		{
			return false;
		}
		for (const FVoxelStructureDetailRuntimeSocket& Socket : Piece.Details)
		{
			if (!CheckStructureCancel(InCancel, OutError))
			{
				return false;
			}
			FVoxelStructureDetailPlacement Detail;
			Detail.DetailId = Socket.DetailId;
			if (!VoxelGeneration::TryWorldCell(PieceOrigin + RotateCorner(FInt64Vector(Socket.SourceCorner), Placement.Yaw),
				Detail.Position))
			{
				continue;
			}
			Detail.Yaw = (Socket.Yaw + Placement.Yaw) & 3;
			Detail.OwnerId = Instance.Id;
			Instance.Details.Add(MoveTemp(Detail));
		}
	}

	/**
	 * 稳定排序。
	 * 后续多 Structure 冲突解决时必须使用：
	 *
	 * Stage -> InstanceId -> Position
	 */
	Instance.Writes.Sort(
		[](const FVoxelStructureCellWrite& InA,
		   const FVoxelStructureCellWrite& InB)
		{
			if (InA.Position.Z != InB.Position.Z)
			{
				return InA.Position.Z < InB.Position.Z;
			}

			if (InA.Position.Y != InB.Position.Y)
			{
				return InA.Position.Y < InB.Position.Y;
			}

			return InA.Position.X < InB.Position.X;
		});

	if (!CheckStructureCancel(InCancel, OutError))
	{
		return false;
	}
	FreeStructureInstance(OutInstance, InOutputWorkspace);
	OutInstance =
		MoveTemp(Instance);

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::BuildPiecePlacements(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FIntVector& InAnchor,
	uint64 InCandidateSeed,
	TArray<FVoxelStructurePiecePlacement>& OutPieces,
	FString& OutError,
	FVoxelGenerationWorkspace& InWorkspace,
	const TAtomic<bool>* InCancel) const
{
	if (!InWorkspace.Check(OutError) || !CheckStructureCancel(InCancel, OutError))
	{
		return false;
	}
	OutPieces.Reset();

	if (!InDefinition.LayoutAlgorithmId.IsNone())
	{
		TSharedPtr<
			const IVoxelStructureLayoutAlgorithm,
			ESPMode::ThreadSafe> Algorithm =
				FVoxelStructureLayoutRegistry::Get().Find(
					InDefinition.LayoutAlgorithmId);

		if (!Algorithm)
		{
			OutError = FString::Printf(
				TEXT("Voxel structure layout algorithm is not registered: %s"),
				*InDefinition.LayoutAlgorithmId.ToString());

			return false;
		}

		if (Algorithm->GetVersion() !=
			InDefinition.LayoutAlgorithmVersion)
		{
			OutError = FString::Printf(
				TEXT("Voxel structure layout algorithm version mismatch: %s recipe=%u runtime=%u"),
				*InDefinition.LayoutAlgorithmId.ToString(),
				InDefinition.LayoutAlgorithmVersion,
				Algorithm->GetVersion());

			return false;
		}

		FVoxelStructureLayoutContext Context;
		Context.WorldSeed =
			Recipe->Settings.Seed;

		Context.Recipe =
			&Recipe.Get();

		Context.Definition =
			&InDefinition;

		Context.Anchor =
			InAnchor;

		Context.CandidateSeed =
			InCandidateSeed;
		Context.Workspace = &InWorkspace;
		Context.Cancel = InCancel;

		const bool bBuilt = Algorithm->BuildLayout(
			Context,
			OutPieces,
			OutError);
		return InWorkspace.Check(OutError) && bBuilt &&
			CheckStructureCancel(InCancel, OutError);
	}

	if (InDefinition.Pieces.IsEmpty())
	{
		OutError = TEXT("Voxel structure has no pieces");
		return false;
	}

	FVoxelStructurePiecePlacement Placement;

	Placement.PieceIndex = 0;
	Placement.Origin =
		FInt64Vector::ZeroValue;

	Placement.Yaw =
		static_cast<uint8>(
			VoxelGeneration::RandomRange(
				VoxelGeneration::Mix(
					InCandidateSeed ^
					0xE5A1B9913CA70281ull),
				0,
				3));

	if (!InWorkspace.ReserveArray(OutPieces, 1, OutError))
	{
		return false;
	}
	OutPieces.Add(
		Placement);

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::RasterizePiece(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FVoxelStructurePiecePlacement& InPlacement,
	const FIntVector& InStructureAnchor,
	TArray<FVoxelStructureCellWrite>& OutWrites,
	TArray<FVoxelStructureClearVolume>& OutClearVolumes,
	FString& OutError,
	FVoxelGenerationWorkspace* InOutputWorkspace,
	const TAtomic<bool>* InCancel) const
{
	if (!CheckStructureAllocation(OutError, InOutputWorkspace) ||
		!CheckStructureCancel(InCancel, OutError))
	{
		return false;
	}
	if (!InDefinition.Pieces.IsValidIndex(
		InPlacement.PieceIndex))
	{
		OutError = TEXT("Voxel structure piece placement references an invalid piece");
		return false;
	}

	const FVoxelStructurePieceTemplate& Piece =
		InDefinition.Pieces[
			InPlacement.PieceIndex];

	const FInt64Vector PieceOrigin =
		FInt64Vector(InStructureAnchor) +
		InPlacement.Origin -
		RotateCorner(
			FInt64Vector(Piece.Entrance),
			InPlacement.Yaw);

	const auto VisitWrites = [&](TFunctionRef<bool(const FVoxelStructureCellWrite&)> Visit)
	{
		for (const FVoxelStructureRun& Run : Piece.Writes)
		{
			for (int32 RunOffset = 0; RunOffset < Run.Length; ++RunOffset)
			{
				if (!CheckStructureCancel(InCancel, OutError))
				{
					return false;
				}
				const FInt64Vector LocalCell = FInt64Vector(Run.Start) + FInt64Vector(RunOffset, 0, 0);
				FVoxelStructureCellWrite Write;
				if (!VoxelGeneration::TryWorldCell(PieceOrigin + RotateCell(LocalCell, InPlacement.Yaw), Write.Position))
				{
					continue;
				}
				Write.Value = Run.Value;
				if (!Visit(Write))
				{
					return false;
				}
			}
		}
		return true;
	};
	const auto VisitClears = [&](TFunctionRef<bool(const FVoxelStructureClearVolume&)> Visit)
	{
		for (const FVoxelGenerationBounds& ClearVolume : Piece.ClearVolumes)
		{
			if (!CheckStructureCancel(InCancel, OutError))
			{
				return false;
			}
			FVoxelStructureClearVolume RuntimeClear;
			if (!TransformBounds(ClearVolume, PieceOrigin, InPlacement.Yaw, RuntimeClear.Bounds))
			{
				continue;
			}
			if (!Visit(RuntimeClear))
			{
				return false;
			}
		}
		return true;
	};
	int64 WriteCount = OutWrites.Num();
	int64 ClearCount = OutClearVolumes.Num();
	if (!VisitWrites([&](const FVoxelStructureCellWrite&)
	{
		if (++WriteCount > MAX_int32)
		{
			return FailStructureAllocation(TEXT("Oversized: structure write count exceeds native capacity"),
				OutError, InOutputWorkspace);
		}
		return true;
	}) || !VisitClears([&](const FVoxelStructureClearVolume&)
	{
		if (++ClearCount > MAX_int32)
		{
			return FailStructureAllocation(TEXT("Oversized: structure clear count exceeds native capacity"),
				OutError, InOutputWorkspace);
		}
		return true;
	}) || !ReserveStructureArray(OutWrites, int32(WriteCount), OutError, InOutputWorkspace) ||
		!ReserveStructureArray(OutClearVolumes, int32(ClearCount), OutError, InOutputWorkspace))
	{
		return false;
	}
	if (!VisitWrites([&](const FVoxelStructureCellWrite& Write)
	{
		OutWrites.Add(Write);
		return true;
	}) || !VisitClears([&](const FVoxelStructureClearVolume& Clear)
	{
		OutClearVolumes.Add(Clear);
		return true;
	}))
	{
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::ResolveTerrainAdaptedAnchor(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FIntVector& InCandidate,
	const FVoxelColumnSample& InColumn,
	FIntVector& OutAnchor) const
{
	FInt64Vector Result(InCandidate);

	switch (InDefinition.Adaptation)
	{
	case EVoxelStructureAdaptation::Bury:
		Result.Z -=
			InDefinition.Placement.MaxTerrainAdjustment;
		break;

	case EVoxelStructureAdaptation::Flatten:
	case EVoxelStructureAdaptation::CutFill:
	case EVoxelStructureAdaptation::Blend:
	case EVoxelStructureAdaptation::Encapsulate:
	case EVoxelStructureAdaptation::None:
	default:
		break;
	}

	return VoxelGeneration::TryWorldCell(Result, OutAnchor);
}
