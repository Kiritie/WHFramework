#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelFeaturePlan.h"
#include "Voxel/Generation/VoxelFeature.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Generation/VoxelStructurePlan.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	class FVoxelTestPointFeatureAlgorithm final : public IVoxelFeatureAlgorithm
	{
	public:
		virtual FName GetId() const override
		{
			return TEXT("test:point_feature_p11");
		}

		virtual uint32 GetVersion() const override
		{
			return 1;
		}

		virtual bool GatherReferencedBlocks(
			const FParameter& InConfiguration,
			TArray<FPrimaryAssetId>& OutBlockAssets,
			FString& OutError) const override
		{
			(void)InConfiguration;
			OutBlockAssets.Reset();
			OutError.Reset();
			return true;
		}

		virtual bool BakeConfiguration(
			const FVoxelFeatureBakeContext& InContext,
			const FParameter& InConfiguration,
			TArray<uint8>& OutBytes,
			FString& OutError) const override
		{
			(void)InContext;
			(void)InConfiguration;
			OutBytes.Reset();
			OutError.Reset();
			return true;
		}

		virtual bool Generate(
			const FVoxelFeatureQueryContext& InContext,
			TConstArrayView<uint8> InConfigBytes,
			FVoxelFeatureInstance& OutInstance,
			FString& OutError) const override
		{
			(void)InConfigBytes;
			OutInstance.Writes.Add({ InContext.CandidateAnchor, 1 });
			OutError.Reset();
			return true;
		}
	};

	bool SampleTestColumn(const FIntVector& InPosition, FVoxelColumnSample& OutColumn)
	{
		(void)InPosition;
		OutColumn.SurfaceZ = 16;
		OutColumn.BiomeIndex = 0;
		return true;
	}

	bool SampleTestSymbol(const FIntVector& InPosition, uint32& OutSymbol)
	{
		(void)InPosition;
		OutSymbol = 1;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStructureFeatureConflictTest,
	"WHFramework.Voxel.Generation.StructureFeatureConflict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStructureFeatureConflictTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FIntVector Position(4, -2, 8);
	const FVoxelStableId Earlier = VoxelGeneration::MakeStableId(7, Position, 1, 1);
	const FVoxelStableId Later = VoxelGeneration::MakeStableId(7, Position, 1, 2);
	const FVoxelStableId Low = Earlier < Later ? Earlier : Later;
	const FVoxelStableId High = Earlier < Later ? Later : Earlier;

	FVoxelStructurePlan Structure;
	Structure.Writes.Add({ Position, 11, EVoxelGenerationStage::SurfaceStructures, Low });
	Structure.Writes.Add({ Position, 22, EVoxelGenerationStage::SurfaceStructures, High });
	Structure.Finalize();
	uint32 Value = 0;
	TestTrue(TEXT("Structure write resolves"), Structure.Sample(Position, EVoxelGenerationStage::SurfaceStructures, Value));
	TestEqual(TEXT("Later stable owner overrides"), Value, uint32(22));
	FVoxelStructurePlanClear Clear;
	Clear.Bounds = { Position, Position + FIntVector(1) };
	Clear.Stage = EVoxelGenerationStage::SurfaceStructures;
	Clear.OwnerId = Low;
	Structure.Clears.Add(Clear);
	TestTrue(TEXT("Clear volume applies before writes"), Structure.IsCleared(Position, EVoxelGenerationStage::SurfaceStructures));

	FVoxelFeaturePlan Feature;
	Feature.Writes.Add({ Position, 31, EVoxelGenerationStage::Vegetation, Low });
	Feature.Writes.Add({ Position, 32, EVoxelGenerationStage::Vegetation, High });
	Feature.Finalize();
	TestTrue(TEXT("Feature write resolves"), Feature.Sample(Position, EVoxelGenerationStage::Vegetation, Value));
	TestEqual(TEXT("Feature stable owner order"), Value, uint32(32));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStructureFeatureOwnershipTest,
	"WHFramework.Voxel.Generation.StructureFeatureOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStructureFeatureOwnershipTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	FVoxelStructureRuntimeDefinition Structure;
	Structure.StableId = TEXT("test:structure");
	Structure.StableHash = 0xa49e8712bd341209ull;
	Structure.Stage = EVoxelGenerationStage::SurfaceStructures;
	Structure.Placement.Spacing = 32;
	Structure.Placement.Separation = 4;
	Structure.Placement.ChancePermille = 1000;
	Structure.Placement.bRequireSurface = true;
	Structure.Adaptation = EVoxelStructureAdaptation::CutFill;
	FVoxelStructurePieceTemplate Piece;
	Piece.StableId = TEXT("test:piece");
	Piece.Bounds = { FIntVector::ZeroValue, FIntVector(1) };
	Piece.Writes.Add({ FIntVector::ZeroValue, 1, 1 });
	Piece.ClearVolumes.Add(Piece.Bounds);
	Structure.Pieces.Add(Piece);
	Recipe.Structures.Add(Structure);
	Recipe.Biomes[0].StructureIndices.Add(0);

	FVoxelFeatureRuntimeDefinition Feature;
	Feature.StableId = TEXT("test:feature");
	Feature.StableHash = 0x9137f224ca68d15bull;
	Feature.AlgorithmId = TEXT("test:point_feature_p11");
	Feature.AlgorithmVersion = 1;
	Feature.Stage = EVoxelGenerationStage::Vegetation;
	Feature.Placement.Spacing = 16;
	Feature.Placement.ChancePermille = 1000;
	Feature.Placement.bRequireSurface = true;
	Recipe.Features.Add(Feature);
	Recipe.Biomes[0].FeatureIndices.Add(0);
	FString Error;
	TestTrue(TEXT("Ownership recipe builds lookups"), Recipe.BuildLookups(Error));
	const TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> SharedRecipe =
		MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe));

	FVoxelStructurePlanner StructurePlanner(SharedRecipe);
	const FVoxelGenerationBounds BoundsA{ FIntVector(0, 0, -32), FIntVector(64, 64, 64) };
	const FVoxelGenerationBounds BoundsB{ FIntVector(16, 16, -32), FIntVector(80, 80, 64) };
	TArray<FVoxelStructureInstance> StructuresA;
	TArray<FVoxelStructureInstance> StructuresB;
	TestTrue(TEXT("First structure query plans"), StructurePlanner.Plan(BoundsA, SampleTestColumn, StructuresA, Error));
	TestTrue(TEXT("Overlapping structure query plans"), StructurePlanner.Plan(BoundsB, SampleTestColumn, StructuresB, Error));
	TSet<FVoxelStableId> StructureIds;
	for (const FVoxelStructureInstance& Instance : StructuresA)
	{
		TestFalse(TEXT("Canonical structure owner appears once"), StructureIds.Contains(Instance.Id));
		StructureIds.Add(Instance.Id);
	}
	int32 SharedStructures = 0;
	for (const FVoxelStructureInstance& Instance : StructuresB)
	{
		SharedStructures += StructureIds.Contains(Instance.Id) ? 1 : 0;
	}
	TestTrue(TEXT("Overlapping structure query retains stable ownership"), SharedStructures > 0);

	FVoxelFeatureAlgorithmRegistry::Get().Unregister(TEXT("test:point_feature_p11"));
	TestTrue(
		TEXT("Point feature algorithm registers"),
		FVoxelFeatureAlgorithmRegistry::Get().Register(
			MakeShared<const FVoxelTestPointFeatureAlgorithm, ESPMode::ThreadSafe>(),
			Error));
	FVoxelFeaturePlanner FeaturePlanner(SharedRecipe);
	TArray<FVoxelFeatureInstance> FeaturesA;
	TArray<FVoxelFeatureInstance> FeaturesB;
	TestTrue(TEXT("First feature query plans"), FeaturePlanner.Plan(BoundsA, SampleTestColumn, SampleTestSymbol, FeaturesA, Error));
	TestTrue(TEXT("Overlapping feature query plans"), FeaturePlanner.Plan(BoundsB, SampleTestColumn, SampleTestSymbol, FeaturesB, Error));
	TSet<FVoxelStableId> FeatureIds;
	for (const FVoxelFeatureInstance& Instance : FeaturesA)
	{
		TestFalse(TEXT("Feature candidate is not duplicated"), FeatureIds.Contains(Instance.Id));
		FeatureIds.Add(Instance.Id);
	}
	int32 SharedFeatures = 0;
	for (const FVoxelFeatureInstance& Instance : FeaturesB)
	{
		SharedFeatures += FeatureIds.Contains(Instance.Id) ? 1 : 0;
	}
	TestTrue(TEXT("Adjacent feature queries retain stable ids"), SharedFeatures > 0);
	FVoxelFeatureAlgorithmRegistry::Get().Unregister(TEXT("test:point_feature_p11"));
	return true;
}

#endif
