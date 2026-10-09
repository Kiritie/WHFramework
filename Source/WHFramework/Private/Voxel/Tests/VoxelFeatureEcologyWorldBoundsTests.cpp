#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/VoxelBuiltinFeatures.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace VoxelFeatureEcologyWorldBoundsTest
{
	constexpr uint64 FeatureSalt = 0x79A20E451AB67231ull;
	constexpr uint64 EcologyTreeSalt = 0x62CEBFAD38B9C4D1ull;
	constexpr uint64 EcologyGrassSalt = 0x1987D4A6723FE901ull;
	constexpr uint64 EcologyFlowerSalt = 0x464C4F5745525041ull;

	struct FScopedOreRegistration
	{
		explicit FScopedOreRegistration(FString& OutError)
		{
			bOwned = !FVoxelFeatureAlgorithmRegistry::Get().Find(FVoxelOreVeinFeatureAlgorithm::AlgorithmId);
			bRegistered = VoxelBuiltinFeatures::Register(OutError);
		}

		~FScopedOreRegistration()
		{
			if (bOwned && bRegistered)
			{
				FVoxelFeatureAlgorithmRegistry::Get().Unregister(FVoxelOreVeinFeatureAlgorithm::AlgorithmId);
			}
		}

		bool bOwned = false;
		bool bRegistered = false;
	};

	bool MakeOreBytes(const int32 InLength, const int32 InRadius, TArray<uint8>& OutBytes)
	{
		FVoxelByteWriter Writer(64);
		Writer.U16(7);
		Writer.U8(1);
		Writer.U16(1);
		Writer.I32(InLength);
		Writer.I32(InLength);
		Writer.I32(InRadius);
		return Writer.Finish(OutBytes);
	}

	int32 FindSeedForOffset(const FIntVector& InGrid, const uint64 InSalt,
		const uint64 InJitterSalt, const int32 InSpacing, const int32 InOffset)
	{
		for (int32 Seed = 1; Seed <= 4096; ++Seed)
		{
			const uint64 Candidate = VoxelGeneration::MakeSeed(Seed, InGrid, InSalt);
			if (VoxelGeneration::RandomRange(VoxelGeneration::Mix(Candidate ^ InJitterSalt), 0, InSpacing - 1) == InOffset)
			{
				return Seed;
			}
		}
		return INDEX_NONE;
	}

	bool IsWorldPosition(const FIntVector& InPosition)
	{
		return InPosition.X < MAX_int32 && InPosition.Y < MAX_int32 && InPosition.Z < MAX_int32;
	}

	bool AreLocalQueries(TConstArrayView<FIntVector> InQueries, const FIntVector& InCenter, const int32 InRadius)
	{
		for (const FIntVector& Position : InQueries)
		{
			if (!IsWorldPosition(Position) || FMath::Abs(static_cast<int64>(Position.X) - InCenter.X) > InRadius ||
				FMath::Abs(static_cast<int64>(Position.Y) - InCenter.Y) > InRadius)
			{
				return false;
			}
		}
		return true;
	}

	FVoxelGenerationRecipe MakeFeatureRecipe(const int32 InSeed)
	{
		FVoxelGenerationRecipe Recipe;
		Recipe.Settings.Seed = InSeed;
		Recipe.Palette.Air = 0;
		Recipe.Palette.Stone = 1;
		Recipe.Biomes.AddDefaulted_GetRef().FeatureIndices.Add(0);
		auto& Feature = Recipe.Features.AddDefaulted_GetRef();
		Feature.StableId = TEXT("test:world-bounds-ore");
		Feature.StableHash = FeatureSalt;
		Feature.AlgorithmId = FVoxelOreVeinFeatureAlgorithm::AlgorithmId;
		Feature.AlgorithmVersion = 1;
		Feature.Placement.Spacing = 3;
		MakeOreBytes(1, 0, Feature.ConfigBytes);
		return Recipe;
	}

	void FillColumn(FVoxelColumnSample& OutColumn, const int32 InSurfaceZ)
	{
		OutColumn.SurfaceZ = InSurfaceZ;
		OutColumn.BiomeIndex = 0;
		OutColumn.SurfaceMaterial = 1;
		OutColumn.SlopePermille = 0;
		OutColumn.Climate.TemperatureQ15 = 0;
		OutColumn.Climate.MoistureQ15 = 12000;
	}

	bool BuildFeature(const FVoxelGenerationRecipe& InRecipe, const FVoxelGenerationBounds& InBounds,
		const int32 InSurfaceZ, TArray<FIntVector>& OutColumnQueries, TArray<FIntVector>& OutBaseQueries,
		TArray<FVoxelFeatureInstance>& OutInstances, FString& OutError)
	{
		const FVoxelFeaturePlanner Planner(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(InRecipe));
		return Planner.Plan(InBounds,
			[&OutColumnQueries, InSurfaceZ](const FIntVector& Position, FVoxelColumnSample& Column)
			{
				OutColumnQueries.Add(Position);
				FillColumn(Column, InSurfaceZ);
				return true;
			},
			[&OutBaseQueries](const FIntVector& Position, uint32& Value)
			{
				OutBaseQueries.Add(Position);
				Value = 1;
				return true;
			}, OutInstances, OutError);
	}

	FVoxelGenerationRecipe MakeEcologyRecipe(const int32 InSeed)
	{
		FVoxelGenerationRecipe Recipe;
		Recipe.Settings.Seed = InSeed;
		Recipe.Palette.Air = 0;
		Recipe.Palette.Grass = 1;
		Recipe.Palette.Sand = 6;
		Recipe.Ecology.TreeTrunk = 2;
		Recipe.Ecology.TreeLeaves = 3;
		Recipe.Ecology.GrassPlant = 4;
		Recipe.Ecology.Flowers.Add({5, 1});
		Recipe.Biomes.AddDefaulted_GetRef().DefaultSurface = 1;
		Recipe.Settings.Ecology.Tree.bEnabled = false;
		Recipe.Settings.Ecology.Grass.bEnabled = false;
		Recipe.Settings.Ecology.Flower.bEnabled = false;
		Recipe.Settings.Ecology.Grass.Spacing = 3;
		Recipe.Settings.Ecology.Grass.PatchRadius = 2;
		Recipe.Settings.Ecology.Grass.ChancePermille = 1000;
		Recipe.Settings.Ecology.Grass.DensityPermille = 1000;
		Recipe.Settings.Ecology.Grass.PatchFillPermille = 1000;
		Recipe.Settings.Ecology.Flower.Spacing = 3;
		Recipe.Settings.Ecology.Flower.PatchRadius = 2;
		Recipe.Settings.Ecology.Flower.ChancePermille = 1000;
		Recipe.Settings.Ecology.Flower.DensityPermille = 1000;
		Recipe.Settings.Ecology.Flower.PatchFillPermille = 1000;
		return Recipe;
	}

	bool BuildEcology(const FVoxelGenerationRecipe& InRecipe, const FVoxelGenerationBounds& InBounds,
		const int32 InSurfaceZ, TArray<FIntVector>& OutColumnQueries, TArray<FIntVector>& OutBaseQueries,
		FVoxelEcologyPlan& OutPlan, FString& OutError)
	{
		const FVoxelEcologyGenerator Generator(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(InRecipe));
		return Generator.BuildPlan(InBounds,
			[&OutColumnQueries, InSurfaceZ](const FIntVector& Position, FVoxelColumnSample& Column)
			{
				OutColumnQueries.Add(Position);
				FillColumn(Column, InSurfaceZ);
				return true;
			},
			[&OutBaseQueries, InSurfaceZ](const FIntVector& Position, uint32& Value)
			{
				OutBaseQueries.Add(Position);
				Value = Position.Z <= InSurfaceZ ? 1 : 0;
				return true;
			}, OutPlan, OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFeatureCandidateWorldBoundsTest,
	"WHFramework.Voxel.Generation.WorldBounds.FeatureCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFeatureCandidateWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	using namespace VoxelFeatureEcologyWorldBoundsTest;
	FString Error;
	const FScopedOreRegistration Registration(Error);
	if (!TestTrue(TEXT("注册原生矿脉算法"), Registration.bRegistered)) return false;
	for (const bool bMinimum : {true, false})
	{
		const int32 CellX = bMinimum ? MIN_int32 : MAX_int32 - 1;
		const FIntVector Grid(static_cast<int32>(VoxelGeneration::FloorDivideWide(CellX, 3)), 0, 0);
		const int32 ValidOffset = bMinimum ? 1 : 0;
		const int32 Seed = FindSeedForOffset(Grid, FeatureSalt, 0x8A27A1255E012A19ull, 3, ValidOffset);
		if (!TestTrue(TEXT("存在保持原grid种子的边缘候选"), Seed != INDEX_NONE)) return false;
		const auto Recipe = MakeFeatureRecipe(Seed);
		const FVoxelGenerationBounds Bounds{{CellX, 0, -1}, {bMinimum ? MIN_int32 + 2 : MAX_int32, 3, 1}};
		TArray<FIntVector> Columns;
		TArray<FIntVector> Bases;
		TArray<FVoxelFeatureInstance> Instances;
		if (!TestTrue(TEXT("合法末端owner构建原生Feature"), BuildFeature(Recipe, Bounds, 0, Columns, Bases, Instances, Error))) return false;
		if (!TestEqual(TEXT("jitter后的合法候选没有因域外grid原点丢失"), Instances.Num(), 1)) return false;
		TestEqual(TEXT("anchor不被Clamp或重映射"), Instances[0].Anchor.X, CellX);
		TestTrue(TEXT("保留原候选稳定身份"), Instances[0].Id == VoxelGeneration::MakeStableId(Seed, Instances[0].Anchor, FeatureSalt));
		TestEqual(TEXT("合法候选只采样原列"), Columns.Num(), 1);
		TestTrue(TEXT("采样只在原候选附近"), AreLocalQueries(Columns, Instances[0].Anchor, 0) && AreLocalQueries(Bases, Instances[0].Anchor, 0));

		const int32 InvalidSeed = FindSeedForOffset(Grid, FeatureSalt, 0x8A27A1255E012A19ull, 3, bMinimum ? 0 : 1);
		if (!TestTrue(TEXT("存在域外jitter候选"), InvalidSeed != INDEX_NONE)) return false;
		Columns.Reset();
		Bases.Reset();
		TestTrue(TEXT("域外候选属于正常跳过"), BuildFeature(MakeFeatureRecipe(InvalidSeed), Bounds, 0, Columns, Bases, Instances, Error));
		TestTrue(TEXT("域外候选不调用任何sampler"), Columns.IsEmpty() && Bases.IsEmpty() && Instances.IsEmpty() && Error.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFeatureSurfaceWorldBoundsTest,
	"WHFramework.Voxel.Generation.WorldBounds.FeatureSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFeatureSurfaceWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	using namespace VoxelFeatureEcologyWorldBoundsTest;
	FString Error;
	const FScopedOreRegistration Registration(Error);
	if (!Registration.bRegistered) return false;
	auto Recipe = MakeFeatureRecipe(31);
	Recipe.Features[0].Placement.bRequireSurface = true;
	Recipe.Features[0].Placement.bRequireSolidFloor = true;
	TArray<FIntVector> Columns;
	TArray<FIntVector> Bases;
	TArray<FVoxelFeatureInstance> Instances;
	for (const int32 SurfaceZ : {MAX_int32 - 1, MAX_int32})
	{
		Columns.Reset();
		TestTrue(TEXT("顶端surface+1越界时正常跳过"), BuildFeature(Recipe, {{0, 0, -1}, {3, 3, MAX_int32}},
			SurfaceZ, Columns, Bases, Instances, Error));
		TestEqual(TEXT("顶端候选确实进入原列采样"), Columns.Num(), 1);
		TestTrue(TEXT("域外surface不送入floor或算法sampler"), Instances.IsEmpty() && Bases.IsEmpty());
	}
	Columns.Reset();
	TestTrue(TEXT("最小合法surface的floor仍可采样"), BuildFeature(Recipe, {{0, 0, MIN_int32}, {3, 3, MIN_int32 + 2}},
		MIN_int32, Columns, Bases, Instances, Error));
	if (!TestEqual(TEXT("底端合法surface生成原矿脉"), Instances.Num(), 1)) return false;
	TestEqual(TEXT("surface+1在宽整数中保持原值"), Instances[0].Anchor.Z, MIN_int32 + 1);
	TestTrue(TEXT("floor保留最小合法cell"), Bases.Contains(FIntVector(Instances[0].Anchor.X, Instances[0].Anchor.Y, MIN_int32)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelOreVeinWorldBoundsTest,
	"WHFramework.Voxel.Generation.WorldBounds.NativeOreVein",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelOreVeinWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	using namespace VoxelFeatureEcologyWorldBoundsTest;
	const FVoxelOreVeinFeatureAlgorithm Algorithm;
	FVoxelFeatureRuntimeDefinition Definition;
	Definition.StableId = TEXT("test:world-bounds-ore");
	Definition.StableHash = FeatureSalt;
	TArray<uint8> Bytes;
	if (!MakeOreBytes(2, 1, Bytes)) return false;
	FString Error;
	for (const bool bMinimum : {true, false})
	{
		const FIntVector Anchor(bMinimum ? MIN_int32 : MAX_int32 - 1);
		int32 WorldSeed = 1;
		while (WorldSeed < 4096 && VoxelGeneration::RandomRange(VoxelGeneration::Mix(
			VoxelGeneration::MakeSeed(WorldSeed, Anchor, FeatureSalt)), 0, 5) != (bMinimum ? 1 : 0)) ++WorldSeed;
		if (!TestTrue(TEXT("存在直接跨出世界的原walk方向"), WorldSeed < 4096)) return false;
		TArray<FIntVector> Samples;
		auto Column = [](const FIntVector&, FVoxelColumnSample&) { return true; };
		auto Base = [&Samples](const FIntVector& Position, uint32& Value)
		{
			Samples.Add(Position);
			Value = 1;
			return true;
		};
		FVoxelFeatureQueryContext Context{WorldSeed, nullptr, &Definition, {}, Anchor, {}, Column, Base};
		FVoxelFeatureInstance Instance;
		TestTrue(TEXT("边缘矿脉保留合法sphere残余"), Algorithm.Generate(Context, Bytes, Instance, Error));
		TestEqual(TEXT("域外sphere voxel不采样且walk越界立即停止"), Samples.Num(), 4);
		TestEqual(TEXT("三个轴均只保留合法半球写入"), Instance.Writes.Num(), 4);
		TestTrue(TEXT("矿脉anchor及原ID不重映射"), Instance.Anchor == Anchor &&
			Instance.Id == VoxelGeneration::MakeStableId(WorldSeed, Anchor, FeatureSalt));
		for (const FIntVector& Position : Samples)
		{
			TestTrue(TEXT("原生矿脉没有采样wrap后的远端cell"), IsWorldPosition(Position) &&
				FMath::Abs(static_cast<int64>(Position.X) - Anchor.X) <= 1 &&
				FMath::Abs(static_cast<int64>(Position.Y) - Anchor.Y) <= 1 &&
				FMath::Abs(static_cast<int64>(Position.Z) - Anchor.Z) <= 1);
		}
	}
	TArray<FIntVector> Samples;
	auto Column = [](const FIntVector&, FVoxelColumnSample&) { return true; };
	auto Base = [&Samples](const FIntVector& Position, uint32& Value)
	{
		Samples.Add(Position);
		Value = 1;
		return true;
	};
	FVoxelFeatureQueryContext InvalidContext{71, nullptr, &Definition, {}, {MAX_int32, 0, 0}, {}, Column, Base};
	FVoxelFeatureInstance Preserved;
	Preserved.Id = {17, 19};
	TestFalse(TEXT("外部非法anchor永久拒收"), Algorithm.Generate(InvalidContext, Bytes, Preserved, Error));
	TestTrue(TEXT("拒收不采样且保留原输出"), Samples.IsEmpty() && Preserved.Id == FVoxelStableId{17, 19});
	MakeOreBytes(1, 1, Bytes);
	FVoxelFeatureQueryContext InteriorContext{71, nullptr, &Definition, {}, {10, 20, 30}, {}, Column, Base};
	TestTrue(TEXT("内点沿用原矿脉生成"), Algorithm.Generate(InteriorContext, Bytes, Preserved, Error));
	TestEqual(TEXT("内点一个完整sphere仍有七格"), Preserved.Writes.Num(), 7);
	TestTrue(TEXT("内点身份保持"), Preserved.Id == VoxelGeneration::MakeStableId(71, {10, 20, 30}, FeatureSalt));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEcologyPatchWorldBoundsTest,
	"WHFramework.Voxel.Generation.WorldBounds.EcologyPatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologyPatchWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	using namespace VoxelFeatureEcologyWorldBoundsTest;
	for (const bool bFlower : {false, true})
	{
		for (const bool bMinimum : {true, false})
		{
			const int32 CellX = bMinimum ? MIN_int32 : MAX_int32 - 1;
			const uint64 Salt = bFlower ? EcologyFlowerSalt : EcologyGrassSalt;
			const FIntVector Grid(static_cast<int32>(VoxelGeneration::FloorDivideWide(CellX, 3)), 0, 0);
			const int32 Seed = FindSeedForOffset(Grid, Salt, 0xFA9170D133E38A21ull, 3, bMinimum ? 1 : 0);
			if (!TestTrue(TEXT("找到原patch seed的合法边缘中心"), Seed != INDEX_NONE)) return false;
			auto Recipe = MakeEcologyRecipe(Seed);
			Recipe.Settings.Ecology.Flower.bEnabled = bFlower;
			Recipe.Settings.Ecology.Grass.bEnabled = !bFlower;
			const uint64 CandidateSeed = VoxelGeneration::MakeSeed(Seed, Grid, Salt);
			const FIntVector Center(CellX, VoxelGeneration::RandomRange(
				VoxelGeneration::Mix(CandidateSeed ^ 0x5A738CB14268F9E1ull), 0, 2), 0);
			const FVoxelStableId OwnerId = VoxelGeneration::MakeStableId(Seed, Center, Salt);
			TArray<FIntVector> Columns;
			TArray<FIntVector> Bases;
			FVoxelEcologyPlan Plan;
			FString Error;
			const FVoxelGenerationBounds Bounds{{CellX, 0, -1}, {bMinimum ? MIN_int32 + 1 : MAX_int32, 3, 2}};
			if (!TestTrue(TEXT("草花真实patch在世界边界裁切"), BuildEcology(Recipe, Bounds, 0, Columns, Bases, Plan, Error))) return false;
			TestEqual(TEXT("五乘五patch保留合法的三列"), Plan.Writes.Num(), 15);
			TestEqual(TEXT("Count与Build各采样合法中心及十五个patch列"), Columns.Num(), 32);
			bool bFrozenColumnsEqual = Columns.Num() == 32;
			for (int32 Index = 0; bFrozenColumnsEqual && Index < 16; ++Index)
				bFrozenColumnsEqual = Columns[Index] == Columns[Index + 16];
			TestTrue(TEXT("两遍冻结采样读取相同的合法列"), bFrozenColumnsEqual);
			TestTrue(TEXT("草花采样没有远端wrap"), AreLocalQueries(Columns, Center, 2) && AreLocalQueries(Bases, Center, 2));
			for (const FVoxelEcologyPlanWrite& Write : Plan.Writes)
			{
				TestTrue(TEXT("裁切保留原patch身份及合法写入"), Write.OwnerId == OwnerId && IsWorldPosition(Write.Position));
			}
			const FIntVector CrossOwner(bMinimum ? MIN_int32 + 1 : MAX_int32 - 2, Center.Y, 1);
			uint32 Value = 0;
			TestTrue(TEXT("裁切世界域仍保留跨owner的有效patch"), !Bounds.Contains(CrossOwner) && Plan.Sample(CrossOwner, Value));
			TestTrue(TEXT("跨owner写入进入实际影响范围"), Plan.AffectsBounds({CrossOwner, CrossOwner + FIntVector(1)}));
		}
	}
	for (const bool bFlower : {false, true})
	{
		auto Recipe = MakeEcologyRecipe(31);
		Recipe.Settings.Ecology.Flower.bEnabled = bFlower;
		Recipe.Settings.Ecology.Grass.bEnabled = !bFlower;
		TArray<FIntVector> Columns;
		TArray<FIntVector> Bases;
		FVoxelEcologyPlan Plan;
		FString Error;
		for (const int32 SurfaceZ : {MAX_int32 - 1, MAX_int32})
		{
			Columns.Reset();
			TestTrue(TEXT("草花surface+1越界时正常完成空写入"), BuildEcology(Recipe,
				{{0, 0, -1}, {3, 3, MAX_int32}}, SurfaceZ, Columns, Bases, Plan, Error));
			TestTrue(TEXT("域外草花surface不送进base sampler"), !Columns.IsEmpty() && Plan.Writes.IsEmpty() && Bases.IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEcologyTreeWorldBoundsTest,
	"WHFramework.Voxel.Generation.WorldBounds.EcologyTrees",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologyTreeWorldBoundsTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	using namespace VoxelFeatureEcologyWorldBoundsTest;
	for (const bool bMinimum : {true, false})
	{
		const int32 CellX = bMinimum ? MIN_int32 : MAX_int32 - 1;
		const FIntVector Grid(static_cast<int32>(VoxelGeneration::FloorDivideWide(CellX, 5)), 0, 0);
		const int32 Seed = FindSeedForOffset(Grid, EcologyTreeSalt, 0x91827211ABF49D33ull, 5, bMinimum ? 2 : 1);
		if (!TestTrue(TEXT("找到原树木seed的边缘anchor"), Seed != INDEX_NONE)) return false;
		auto Recipe = MakeEcologyRecipe(Seed);
		auto& Tree = Recipe.Settings.Ecology.Tree;
		Tree.bEnabled = true;
		Tree.Spacing = 5;
		Tree.CrownRadius = 2;
		Tree.ChancePermille = 1000;
		Tree.DensityPermille = 1000;
		Tree.MinHeight = bMinimum ? 4 : 1;
		Tree.MaxHeight = Tree.MinHeight;
		auto& Species = Recipe.Ecology.TreeSpecies.AddDefaulted_GetRef();
		Species.StableId = TEXT("test:world-bounds-tree");
		Species.Trunk = 2;
		Species.LeavesDark = 3;
		Species.LeavesMid = 3;
		Species.LeavesLight = 3;
		// 残余叶片断言使用确定冠形，避免 Broadleaf 的旋转与稀疏规则影响边界验证。
		Species.Form = EVoxelTreeForm::Conifer;
		const int32 SurfaceZ = bMinimum ? MAX_int32 - 4 : MIN_int32;
		const uint64 CandidateSeed = VoxelGeneration::MakeSeed(Seed, Grid, EcologyTreeSalt);
		const FIntVector Anchor(CellX, VoxelGeneration::RandomRange(
			VoxelGeneration::Mix(CandidateSeed ^ 0x42CF9A177A16C593ull), 0, 4), SurfaceZ + 1);
		const FVoxelStableId OwnerId = VoxelGeneration::MakeStableId(Seed, Anchor, EcologyTreeSalt);
		TArray<FIntVector> Columns;
		TArray<FIntVector> Bases;
		FVoxelEcologyPlan Plan;
		FString Error;
		const FVoxelGenerationBounds Bounds{{CellX, 0, MIN_int32}, {bMinimum ? MIN_int32 + 1 : MAX_int32, 5, MAX_int32}};
		if (!TestTrue(TEXT("真实树干、枝与冠层按世界域生成"), BuildEcology(Recipe, Bounds, SurfaceZ, Columns, Bases, Plan, Error))) return false;
		TestEqual(TEXT("边缘树保持原候选接纳"), Plan.TreesAccepted, 1);
		int32 TrunkCells = 0;
		for (const FVoxelEcologyPlanWrite& Write : Plan.Writes)
		{
			TestTrue(TEXT("每个树木write保持原ID且没有XYZ wrap"), Write.OwnerId == OwnerId && IsWorldPosition(Write.Position) &&
				FMath::Abs(static_cast<int64>(Write.Position.X) - Anchor.X) <= 2 &&
				FMath::Abs(static_cast<int64>(Write.Position.Y) - Anchor.Y) <= 2 &&
				FMath::Abs(static_cast<int64>(Write.Position.Z) - Anchor.Z) <= 5);
			if (Write.Position.X == Anchor.X && Write.Position.Y == Anchor.Y && Write.Value == 2) ++TrunkCells;
		}
		TestEqual(TEXT("顶端树干裁切且底端树干保留"), TrunkCells, bMinimum ? 3 : 1);
		TestTrue(TEXT("树木sampler只读取合法原位置"), AreLocalQueries(Columns, Anchor, 0) && AreLocalQueries(Bases, Anchor, 0));
		const FIntVector Leaf(bMinimum ? MIN_int32 + 1 : MAX_int32 - 2, Anchor.Y, bMinimum ? MAX_int32 - 1 : MIN_int32);
		uint32 Value = 0;
		TestTrue(TEXT("域外冠中心及边缘offset仍保留合法残余叶片"), Plan.Sample(Leaf, Value) && Value == 3);
		TestTrue(TEXT("有效跨owner冠层计入实际影响范围"), !Bounds.Contains(Leaf) &&
			Plan.AffectsBounds({Leaf, Leaf + FIntVector(1)}));
		TestFalse(TEXT("最小int32局部offset不因Abs溢出进入树冠"), FVoxelEcologyGenerator::ContainsCrown({MIN_int32, 0, 0}, 2, OwnerId));
		if (bMinimum)
		{
			Species.Form = EVoxelTreeForm::Broadleaf;
			Tree.MinHeight = 20;
			Tree.MaxHeight = 20;
			Columns.Reset();
			Bases.Reset();
			TestTrue(TEXT("树冠全部域外时保留原合法树干与根枝"), BuildEcology(Recipe, Bounds, SurfaceZ, Columns, Bases, Plan, Error));
			TestTrue(TEXT("合法根枝保持原位置"), Plan.Sample({MIN_int32 + 1, Anchor.Y, Anchor.Z}, Value) && Value == 2);
			for (const FVoxelEcologyPlanWrite& Write : Plan.Writes)
			{
				TestTrue(TEXT("高树冠和分支不wrap到世界另一端"), Write.OwnerId == OwnerId && Write.Value == 2 &&
					IsWorldPosition(Write.Position) && FMath::Abs(static_cast<int64>(Write.Position.Z) - Anchor.Z) <= 2);
			}
		}
		else
		{
			Species.Form = EVoxelTreeForm::Broadleaf;
			Tree.MinHeight = 2;
			Tree.MaxHeight = 2;
			Columns.Reset();
			Bases.Reset();
			TestTrue(TEXT("底端宽叶树分别保留合法分支并过滤域外分支"), BuildEcology(Recipe, Bounds, SurfaceZ, Columns, Bases, Plan, Error));
			TestTrue(TEXT("第二段分支保留原坐标而不重映射"), Plan.Sample({MAX_int32 - 3, Anchor.Y, MIN_int32}, Value) && Value == 2);
			for (const FVoxelEcologyPlanWrite& Write : Plan.Writes)
			{
				TestTrue(TEXT("底端树枝与冠层均在原世界域且保持身份"), Write.OwnerId == OwnerId && IsWorldPosition(Write.Position));
			}
		}
	}
	auto Recipe = MakeEcologyRecipe(31);
	Recipe.Settings.Ecology.Tree.bEnabled = true;
	Recipe.Settings.Ecology.Tree.ChancePermille = 1000;
	Recipe.Settings.Ecology.Tree.Spacing = 5;
	Recipe.Settings.Ecology.Tree.CrownRadius = 2;
	for (const int32 SurfaceZ : {MAX_int32 - 1, MAX_int32})
	{
		TArray<FIntVector> Columns;
		TArray<FIntVector> Bases;
		FVoxelEcologyPlan Plan;
		FString Error;
		TestTrue(TEXT("树木域外surface属于正常跳过"), BuildEcology(Recipe,
			{{0, 0, -1}, {5, 5, MAX_int32}}, SurfaceZ, Columns, Bases, Plan, Error));
		TestEqual(TEXT("树木候选确实进入原列采样"), Plan.TreeCandidates, 1);
		TestTrue(TEXT("域外树木surface不送进base sampler"), Plan.TreesAccepted == 0 && Plan.Writes.IsEmpty() && Bases.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEcologyFinalizedWorldDomainTest,
	"WHFramework.Voxel.Generation.WorldBounds.EcologyFinalize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEcologyFinalizedWorldDomainTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelEcologyPlan Plan;
	const FVoxelStableId OwnerId{31, 37};
	Plan.Writes.Add({{MIN_int32, 0, 0}, 3, 2, OwnerId});
	Plan.Writes.Add({{MAX_int32 - 1, 0, 0}, 3, 2, OwnerId});
	Plan.Writes.Add({{MAX_int32, 0, 0}, 3, 2, OwnerId});
	Plan.Writes.Add({{0, MAX_int32, 0}, 3, 2, OwnerId});
	Plan.Writes.Add({{0, 0, MAX_int32}, 3, 2, OwnerId});
	Plan.Finalize();
	TestEqual(TEXT("Finalize只索引合法cell"), Plan.Writes.Num(), 2);
	uint32 Value = 0;
	TestTrue(TEXT("最小cell仍可消费"), Plan.Sample({MIN_int32, 0, 0}, Value));
	TestFalse(TEXT("排他上界cell不进入正文索引"), Plan.Sample({MAX_int32, 0, 0}, Value));
	TestTrue(TEXT("合法末端write的半开几何保留MAX上界"),
		Plan.AffectsBounds({{MAX_int32 - 1, 0, 0}, {MAX_int32, 1, 1}}));
	return true;
}

#endif
