#include "Voxel/Generation/VoxelFeature.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelFeaturePlan.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Runtime/VoxelRegistry.h"

bool FVoxelFeatureBakeContext::ResolveBlockSymbol(
	const FPrimaryAssetId& InAssetId,
	uint16& OutSymbol,
	FString& OutError) const
{
	if (!Registry || !BlockSymbols || !InAssetId.IsValid())
	{
		OutError = TEXT("Voxel feature bake context is invalid");
		return false;
	}

	const FVoxelRuntimeDefinition* Definition = Registry->Find(InAssetId);
	if (!Definition || Definition->BlockName.IsNone())
	{
		OutError = FString::Printf(TEXT("Voxel feature references an unknown block asset: %s"), *InAssetId.ToString());
		return false;
	}

	const uint16* Symbol = BlockSymbols->Find(Definition->BlockName);
	if (!Symbol)
	{
		OutError = FString::Printf(TEXT("Voxel feature block was not gathered into recipe symbols: %s"), *Definition->BlockName.ToString());
		return false;
	}

	OutSymbol = *Symbol;
	OutError.Reset();
	return true;
}

uint64 FVoxelFeatureInstance::GetAllocatedBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Writes);
}

FVoxelFeatureAlgorithmRegistry&
FVoxelFeatureAlgorithmRegistry::Get()
{
	static FVoxelFeatureAlgorithmRegistry Instance;
	return Instance;
}

bool FVoxelFeatureAlgorithmRegistry::Register(
	TSharedRef<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe> InAlgorithm,
	FString& OutError)
{
	const FName AlgorithmId =
		InAlgorithm->GetId();

	if (AlgorithmId.IsNone())
	{
		OutError = TEXT("Voxel feature algorithm id cannot be None");
		return false;
	}

	if (InAlgorithm->GetVersion() == 0)
	{
		OutError = FString::Printf(
			TEXT("Voxel feature algorithm %s has version zero"),
			*AlgorithmId.ToString());

		return false;
	}

	FWriteScopeLock ScopeLock(Lock);

	if (Algorithms.Contains(AlgorithmId))
	{
		OutError = FString::Printf(
			TEXT("Voxel feature algorithm is already registered: %s"),
			*AlgorithmId.ToString());

		return false;
	}

	Algorithms.Add(
		AlgorithmId,
		MoveTemp(InAlgorithm));

	OutError.Reset();
	return true;
}

void FVoxelFeatureAlgorithmRegistry::Unregister(
	FName InAlgorithmId)
{
	FWriteScopeLock ScopeLock(Lock);
	Algorithms.Remove(InAlgorithmId);
}

TSharedPtr<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe>
FVoxelFeatureAlgorithmRegistry::Find(
	FName InAlgorithmId) const
{
	FReadScopeLock ScopeLock(Lock);

	const TSharedRef<
		const IVoxelFeatureAlgorithm,
		ESPMode::ThreadSafe>* Found =
			Algorithms.Find(InAlgorithmId);

	return Found
		? TSharedPtr<
			const IVoxelFeatureAlgorithm,
			ESPMode::ThreadSafe>(*Found)
		: nullptr;
}

void FVoxelFeatureAlgorithmRegistry::Reset()
{
	FWriteScopeLock ScopeLock(Lock);
	Algorithms.Reset();
}

FVoxelFeaturePlanner::FVoxelFeaturePlanner(
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
	: Recipe(InRecipe)
{
}

namespace
{
	void FreeFeatureInstances(
		TArray<FVoxelFeatureInstance>& InInstances,
		FVoxelGenerationWorkspace* InWorkspace)
	{
		for (FVoxelFeatureInstance& Instance : InInstances)
		{
			if (InWorkspace)
			{
				InWorkspace->FreeArray(Instance.Writes);
			}
			else
			{
				VoxelGenerationAllocation::FreeRetainedArray(Instance.Writes);
			}
		}
		if (InWorkspace)
		{
			InWorkspace->FreeArray(InInstances);
		}
		else
		{
			VoxelGenerationAllocation::FreeRetainedArray(InInstances);
		}
	}
}

bool FVoxelFeatureQueryContext::ReserveWrites(
	TArray<FVoxelFeatureCellWrite>& InWrites,
	const int32 InCapacity,
	FString& OutError) const
{
	return Workspace
		? Workspace->ReserveArray(InWrites, InCapacity, OutError)
		: VoxelGenerationAllocation::RetainArray(InWrites, InCapacity, OutError);
}

void FVoxelFeatureQueryContext::FreeWrites(TArray<FVoxelFeatureCellWrite>& InWrites) const
{
	if (Workspace)
	{
		Workspace->FreeArray(InWrites);
	}
	else
	{
		VoxelGenerationAllocation::FreeRetainedArray(InWrites);
	}
}

bool FVoxelFeaturePlanner::BuildPlan(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
	TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
	FVoxelFeaturePlan& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelGenerationWorkspace Workspace;
	TArray<FVoxelFeatureInstance> Instances;
	ON_SCOPE_EXIT
	{
		FreeFeatureInstances(Instances, &Workspace);
	};
	FVoxelFeaturePlan Result;
	ON_SCOPE_EXIT
	{
		Result.ReleaseBacking();
	};
	if (!Plan(InBounds, InSampleColumn, InSampleBaseSymbol, Instances, OutError, InCancel, &Workspace))
	{
		return false;
	}
	int64 WriteCount = 0;
	for (const FVoxelFeatureInstance& Instance : Instances)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (Recipe->Features.IsValidIndex(Recipe->FindFeature(Instance.DefinitionId)))
		{
			WriteCount += Instance.Writes.Num();
			if (WriteCount > MAX_int32)
			{
				return Workspace.Fail(TEXT("Oversized: feature plan writes exceed native slot domain"), OutError);
			}
		}
	}
	if (!VoxelGenerationAllocation::RetainArray(Result.Writes, int32(WriteCount), OutError))
	{
		return false;
	}
	Result.Bounds = InBounds;
	for (const FVoxelFeatureInstance& Instance : Instances)
	{
		const int32 DefinitionIndex = Recipe->FindFeature(Instance.DefinitionId);
		if (!Recipe->Features.IsValidIndex(DefinitionIndex))
		{
			continue;
		}
		const EVoxelGenerationStage Stage = Recipe->Features[DefinitionIndex].Stage;
		for (const FVoxelFeatureCellWrite& Write : Instance.Writes)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			// owner 瓦片只决定实例归属，所有合法跨瓦片写入完整保留。
			Result.Writes.Add({ Write.Position, Write.Value, Stage, Instance.Id });
		}
	}
	// flatten 已完成，先销毁复制源 native backing，再退 scratch；Finalize 不再占用实例副本。
	FreeFeatureInstances(Instances, &Workspace);
	if (!Result.Finalize(OutError, InCancel))
	{
		return false;
	}
	OutPlan.ReleaseBacking();
	OutPlan = MoveTemp(Result);
	OutError.Reset();
	return true;
}

bool FVoxelFeaturePlanner::Plan(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
	TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
	TArray<FVoxelFeatureInstance>& OutInstances,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	FVoxelGenerationWorkspace* InOutputWorkspace) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_FeaturePlan);
	if (!InBounds.IsValid())
	{
		OutError = TEXT("Voxel feature planner received invalid bounds");
		return false;
	}
	FVoxelGenerationWorkspace Validation;
	if (!(InOutputWorkspace ? InOutputWorkspace->Check(OutError) : Validation.Check(OutError)))
	{
		return false;
	}
	TArray<FVoxelFeatureInstance> Instances;
	ON_SCOPE_EXIT
	{
		FreeFeatureInstances(Instances, InOutputWorkspace);
	};
	for (const FVoxelFeatureRuntimeDefinition& Definition : Recipe->Features)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		const TSharedPtr<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe> Algorithm =
			FVoxelFeatureAlgorithmRegistry::Get().Find(Definition.AlgorithmId);
		if (!Algorithm)
		{
			OutError = FString::Printf(TEXT("Voxel feature algorithm is not registered: %s"), *Definition.AlgorithmId.ToString());
			return false;
		}
		if (Algorithm->GetVersion() != Definition.AlgorithmVersion)
		{
			OutError = FString::Printf(TEXT("Voxel feature algorithm version mismatch: %s recipe=%u runtime=%u"),
				*Definition.AlgorithmId.ToString(), Definition.AlgorithmVersion, Algorithm->GetVersion());
			return false;
		}
		FVoxelGenerationWorkspace AnchorWorkspace;
		TArray<FIntVector> Anchors;
		if (!GatherCandidateAnchors(Definition, InBounds, Anchors, AnchorWorkspace, OutError, InCancel))
		{
			return false;
		}
		for (const FIntVector& Anchor : Anchors)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			FVoxelColumnSample Column;
			if (!InSampleColumn(Anchor, Column) || !Recipe->Biomes.IsValidIndex(Column.BiomeIndex))
			{
				continue;
			}
			const FVoxelBiomeRuntimeDefinition& Biome = Recipe->Biomes[Column.BiomeIndex];
			const int32 DefinitionIndex = int32(&Definition - Recipe->Features.GetData());
			if (!Biome.FeatureIndices.Contains(DefinitionIndex) || !IsPlacementAllowed(Definition, Anchor, Column))
			{
				continue;
			}
			FIntVector SurfaceAnchor;
			if (!VoxelGeneration::TryWorldCell(FInt64Vector(Anchor.X, Anchor.Y,
				Definition.Placement.bRequireSurface ? int64(Column.SurfaceZ) + 1 : Anchor.Z), SurfaceAnchor))
			{
				continue;
			}
			if (Definition.Placement.bRequireSolidFloor)
			{
				FIntVector FloorPosition;
				uint32 FloorValue = 0;
				if (!VoxelGeneration::TryWorldCell(FInt64Vector(SurfaceAnchor) - FInt64Vector(0, 0, 1), FloorPosition) ||
					!InSampleBaseSymbol(FloorPosition, FloorValue) ||
					uint16(FloorValue & 0xffffu) == Recipe->Palette.Air)
				{
					continue;
				}
			}
			const uint64 CandidateSeed = VoxelGeneration::MakeSeed(
				Recipe->Settings.Seed, SurfaceAnchor, Definition.StableHash);
			if (VoxelGeneration::RandomRange(CandidateSeed, 0, 999) >= Definition.Placement.ChancePermille)
			{
				continue;
			}
			FVoxelFeatureQueryContext Context
			{
				Recipe->Settings.Seed, &Recipe.Get(), &Definition, InBounds, SurfaceAnchor, Column,
				InSampleColumn, InSampleBaseSymbol, InOutputWorkspace, InCancel
			};
			FVoxelFeatureInstance Instance;
			ON_SCOPE_EXIT
			{
				Context.FreeWrites(Instance.Writes);
			};
			if (!Algorithm->Generate(Context, Definition.ConfigBytes, Instance, OutError) ||
				!(InOutputWorkspace ? InOutputWorkspace->Check(OutError) : Validation.Check(OutError)))
			{
				return false;
			}
			if (!Instance.Id.IsValid())
			{
				Instance.Id = VoxelGeneration::MakeStableId(Recipe->Settings.Seed, SurfaceAnchor, Definition.StableHash);
			}
			if (Instance.DefinitionId.IsNone())
			{
				Instance.DefinitionId = Definition.StableId;
			}
			Instance.Anchor = SurfaceAnchor;
			Instance.Writes.RemoveAll([](const FVoxelFeatureCellWrite& Write)
			{
				return !VoxelGeneration::IsWorldCell(Write.Position.X) ||
					!VoxelGeneration::IsWorldCell(Write.Position.Y) ||
					!VoxelGeneration::IsWorldCell(Write.Position.Z);
			});
			bool bTouchesOwner = false;
			for (const FVoxelFeatureCellWrite& Write : Instance.Writes)
			{
				if (InBounds.Contains(Write.Position))
				{
					bTouchesOwner = true;
					break;
				}
			}
			if (bTouchesOwner)
			{
				if (Instances.Num() == MAX_int32)
				{
					return Validation.Fail(TEXT("Oversized: feature instances exceed native slot domain"), OutError);
				}
				const bool bReserved = InOutputWorkspace
					? InOutputWorkspace->ReserveArray(Instances, Instances.Num() + 1, OutError)
					: VoxelGenerationAllocation::RetainArray(Instances, Instances.Num() + 1, OutError);
				if (!bReserved)
				{
					return false;
				}
				// 所有嵌套 backing 同属输出预算，Move 不产生未授予的 Writes 副本。
				Instances.Add(MoveTemp(Instance));
			}
		}
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	Instances.Sort([](const FVoxelFeatureInstance& InA, const FVoxelFeatureInstance& InB)
	{
		return InA.Id < InB.Id;
	});
	FreeFeatureInstances(OutInstances, InOutputWorkspace);
	OutInstances = MoveTemp(Instances);
	OutError.Reset();
	return true;
}

bool FVoxelFeaturePlanner::GatherCandidateAnchors(
	const FVoxelFeatureRuntimeDefinition& InDefinition,
	const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutAnchors,
	FVoxelGenerationWorkspace& InWorkspace,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	const int32 Spacing = FMath::Max(1, InDefinition.Placement.Spacing);
	const int64 MinGridX = VoxelGeneration::FloorDivideWide(InBounds.Min.X, Spacing);
	const int64 MaxGridX = VoxelGeneration::FloorDivideWide(int64(InBounds.Max.X) - 1, Spacing);
	const int64 MinGridY = VoxelGeneration::FloorDivideWide(InBounds.Min.Y, Spacing);
	const int64 MaxGridY = VoxelGeneration::FloorDivideWide(int64(InBounds.Max.Y) - 1, Spacing);
	const auto EmitAnchors = [&](auto&& Visitor)
	{
		for (int64 GridY = MinGridY; GridY <= MaxGridY; ++GridY)
		{
			for (int64 GridX = MinGridX; GridX <= MaxGridX; ++GridX)
			{
				if (InCancel && InCancel->Load())
				{
					OutError = TEXT("Canceled");
					return false;
				}
				const FIntVector Grid(int32(GridX), int32(GridY), 0);
				const uint64 Seed = VoxelGeneration::MakeSeed(Recipe->Settings.Seed, Grid, InDefinition.StableHash);
				const int32 OffsetX = VoxelGeneration::RandomRange(
					VoxelGeneration::Mix(Seed ^ 0x8A27A1255E012A19ull), 0, Spacing - 1);
				const int32 OffsetY = VoxelGeneration::RandomRange(
					VoxelGeneration::Mix(Seed ^ 0x9125F48A3CC80177ull), 0, Spacing - 1);
				FIntVector Anchor;
				// lattice 身份不随世界裁切改变；只在最终 cell 收窄前过滤。
				if (VoxelGeneration::TryWorldCell(FInt64Vector(
					GridX * Spacing + OffsetX, GridY * Spacing + OffsetY, 0), Anchor) && !Visitor(Anchor))
				{
					return false;
				}
			}
		}
		return true;
	};
	int32 Count = 0;
	if (!EmitAnchors([&](const FIntVector&)
	{
		if (Count == MAX_int32)
		{
			return InWorkspace.Fail(TEXT("Oversized: feature anchors exceed native slot domain"), OutError);
		}
		++Count;
		return true;
	}) || !InWorkspace.ReserveArray(OutAnchors, Count, OutError))
	{
		return false;
	}
	OutAnchors.Reset();
	if (!EmitAnchors([&](const FIntVector& Anchor)
	{
		OutAnchors.Add(Anchor);
		return true;
	}))
	{
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelFeaturePlanner::IsPlacementAllowed(
	const FVoxelFeatureRuntimeDefinition& InDefinition,
	const FIntVector& InAnchor,
	const FVoxelColumnSample& InColumn) const
{
	const FVoxelFeaturePlacement& Placement =
		InDefinition.Placement;

	const int64 CandidateZ =
		Placement.bRequireSurface
			? static_cast<int64>(InColumn.SurfaceZ) + 1
			: InAnchor.Z;

	if (!VoxelGeneration::IsWorldCell(CandidateZ) ||
		CandidateZ < Placement.MinZ ||
		CandidateZ > Placement.MaxZ)
	{
		return false;
	}

	if (InColumn.SlopePermille >
		Placement.MaxSlopePermille)
	{
		return false;
	}

	if (!Placement.bAllowNearWater &&
		(InColumn.bRiver ||
		 InColumn.bLake ||
		 InColumn.bOcean ||
		 InColumn.bCoast))
	{
		return false;
	}

	return true;
}
