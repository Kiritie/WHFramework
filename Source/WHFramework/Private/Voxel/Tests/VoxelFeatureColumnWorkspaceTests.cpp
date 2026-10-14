#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelFeature.h"
#include "Voxel/Generation/VoxelFeaturePlan.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	class FFeatureColumnProbe final : public IVoxelFeatureAlgorithm
	{
	public:
		FFeatureColumnProbe(FAutomationTestBase& InTest, TAtomic<bool>& InCancel)
			: Test(InTest), Cancel(InCancel)
		{
		}

		virtual FName GetId() const override { return TEXT("test:feature_column_workspace"); }
		virtual uint32 GetVersion() const override { return 1; }
		virtual bool GatherReferencedBlocks(const FParameter&, TArray<FPrimaryAssetId>& OutAssets, FString& OutError) const override
		{
			OutAssets.Reset(); OutError.Reset(); return true;
		}
		virtual bool BakeConfiguration(const FVoxelFeatureBakeContext&, const FParameter&, TArray<uint8>& OutBytes, FString& OutError) const override
		{
			OutBytes.Reset(); OutError.Reset(); return true;
		}
		virtual bool Generate(const FVoxelFeatureQueryContext& Context, TConstArrayView<uint8>,
			FVoxelFeatureInstance& OutInstance, FString& OutError) const override
		{
			++Calls;
			FVoxelColumnSample Column;
			if (!Context.SampleColumn(Context.CandidateAnchor, Column)) return false;
			if (MaintenanceCache && Mode == 0)
			{
				const auto Key = FVoxelGenerationQuery::HydrologyKeyForVoxel(Context.CandidateAnchor.X, Context.CandidateAnchor.Y, Context.Recipe->Settings, Context.Recipe->AlgorithmVersion);
				FVoxelHydrologyPlanPtr Before, After;
				Test.TestTrue(TEXT("Actual natural sampling publishes its original hydrology dependency"), MaintenanceCache->FindHydrology(Key, Before));
				for (int32 Shard = 0; Shard < FVoxelGenerationPlanCache::ShardCount; ++Shard) MaintenanceCache->TickMaintenance(4096);
				Test.TestTrue(TEXT("Native maintenance keeps the same hydrology body throughout a cold feature build"),
					MaintenanceCache->FindHydrology(Key, After) && Before == After);
			}
			if (Mode)
			{
				if (Mode == 1) Cancel.Store(true);
				else FVoxelGenerationPlanningReadScope::Missing(EVoxelGenerationPlanKind::Ecology, Context.CandidateAnchor);
				Test.TestFalse(TEXT("A cached successful column cannot hide cancellation or a missing dependency"),
					Context.SampleColumn(Context.CandidateAnchor + FIntVector(0, 0, 1), Column));
				OutError = Mode == 1 ? TEXT("Canceled") : TEXT("DependencyNotReady: feature probe");
				return false;
			}
			uint32 Below = 0, Above = 0;
			if (!Context.SampleBaseSymbol(FIntVector(Context.CandidateAnchor.X, Context.CandidateAnchor.Y, Column.SurfaceZ - 1024), Below) ||
				!Context.SampleBaseSymbol(FIntVector(Context.CandidateAnchor.X, Context.CandidateAnchor.Y, Column.SurfaceZ + 1024), Above)) return false;
			Test.TestEqual(TEXT("Same XY below ground still computes its own solid density"), Below, uint32(Context.Recipe->Palette.Stone));
			Test.TestEqual(TEXT("Same XY above ground still computes its own air density"), Above, uint32(Context.Recipe->Palette.Air));
			if (!Context.ReserveWrites(OutInstance.Writes, 1, OutError)) return false;
			OutInstance.Writes.Add({Context.CandidateAnchor, Below});
			return true;
		}

		int32 Mode = 0;
		mutable int32 Calls = 0;
		TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> MaintenanceCache;

	private:
		FAutomationTestBase& Test;
		TAtomic<bool>& Cancel;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelFeatureColumnWorkspaceTest,
	"WHFramework.Voxel.Generation.FeatureColumnWorkspaceAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFeatureColumnWorkspaceTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TAtomic<bool> Cancel(false);
	const auto Probe = MakeShared<FFeatureColumnProbe, ESPMode::ThreadSafe>(*this, Cancel);
	FString Error;
	if (!TestTrue(TEXT("Fixture registers its independent algorithm"), FVoxelFeatureAlgorithmRegistry::Get().Register(Probe, Error))) return false;
	ON_SCOPE_EXIT { FVoxelFeatureAlgorithmRegistry::Get().Unregister(Probe->GetId()); };
	FVoxelGenerationRuntimeConfig Author = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe Recipe = *Author.Recipe;
	FVoxelFeatureRuntimeDefinition& Definition = Recipe.Features.AddDefaulted_GetRef();
	Definition.StableId = Probe->GetId();
	Definition.StableHash = 0x3ac1ab76549ef317ull;
	Definition.AlgorithmId = Probe->GetId();
	Definition.AlgorithmVersion = 1;
	Definition.Stage = EVoxelGenerationStage::UndergroundOres;
	Definition.Placement.Spacing = 128;
	Definition.Placement.ChancePermille = 1000;
	Definition.Placement.bRequireSurface = false;
	Definition.Placement.bRequireSolidFloor = false;
	Definition.Placement.bAllowNearWater = true;
	Definition.Placement.MaxSlopePermille = MAX_int32;
	Definition.Placement.MinZ = -2048;
	Definition.Placement.MaxZ = 2048;
	Recipe.Biomes[0].FeatureIndices.Add(0);
	if (!Recipe.BuildLookups(Error)) { AddError(Error); return false; }
	Author.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));
	const auto Consumer = FVoxelGenerationConsumerKey::Section(89, FGuid::NewGuid(), 1, {-1, -1, 0}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	if (!TestTrue(TEXT("Actual frozen configuration and cache are funded by the original native root"),
		FVoxelTerrainResources::Create({}, Consumer, Resources, Error) &&
		FVoxelGenerationBinding::Freeze(Author, Resources.ToSharedRef(), Consumer, Config, Error) &&
		FVoxelGenerationPlanCache::Create(Resources.ToSharedRef(), Consumer, Cache, Error, true, false))) return false;
	FVoxelGenerationConsumerScope Identity(Consumer);
	FVoxelGenerationCacheRetention Retention;
	Retention.Revision = 1;
	Retention.HydrologyRegionSide = Config->Recipe->Settings.HydrologyRegionSide;
	Retention.HydrologyCellSize = Config->Recipe->Settings.HydrologyCellSize;
	if (!TestTrue(TEXT("Native maintenance runs with no world retention points"), Cache->UpdateRetention(Retention, 0,
		[](TArrayView<FVoxelGenerationCacheRetentionPoint>, FString&) { return true; }, Error))) return false;
	Probe->MaintenanceCache = Cache;
	FVoxelGenerationQuery Query;
	if (!TestTrue(TEXT("Production query keeps global column caching disabled"),
		FVoxelGenerationQuery::Create(Config.ToSharedRef(), Cache.ToSharedRef(), Query, Error, false, false, false, &Consumer))) return false;
	FVoxelFeaturePlanPtr Warm;
	if (!TestTrue(TEXT("Production feature build retains every actual write"), Query.EnsureFeaturePlan({FIntVector(-1, -1, 0)}, Warm, Error, &Cancel)))
	{
		AddError(Error); return false;
	}
	TestEqual(TEXT("All four owner anchors ran the actual column and density probe"), Probe->Calls, 4);
	TestEqual(TEXT("Published immutable plan keeps all four writes"), Warm->Writes.Num(), 4);
	TestEqual(TEXT("Operation-local cache releases all original Working backing"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	TestEqual(TEXT("Planning samples do not enter the global natural-column cache"), Cache->GetStats().NaturalColumns, 0);
	for (int32 Shard = 0; Shard < FVoxelGenerationPlanCache::ShardCount; ++Shard) Cache->TickMaintenance(4096);
	TestEqual(TEXT("Completed feature build releases hydrology retention and permits actual eviction"), Cache->GetStats().Hydrology, 0);
	for (Probe->Mode = 1; Probe->Mode <= 2; ++Probe->Mode)
	{
		Cancel.Store(false);
		FVoxelFeaturePlanPtr Failed = Warm;
		{
			FVoxelGenerationPlanningReadScope Read(nullptr, nullptr, &Cancel, &Consumer);
			TestFalse(TEXT("Cached sampling cannot publish canceled or incomplete native facts"),
				Query.EnsureFeaturePlan({FIntVector(-1 - Probe->Mode, -1, 0)}, Failed, Error, &Cancel));
		}
		TestTrue(TEXT("Original blocker remains explicit and old immutable output is preserved"),
			Failed == Warm && (Probe->Mode == 1 ? Error == TEXT("Canceled") : Error.StartsWith(TEXT("DependencyNotReady"))));
		TestEqual(TEXT("Rejected candidates release their entire Working cache"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	}
	return true;
}

#endif
