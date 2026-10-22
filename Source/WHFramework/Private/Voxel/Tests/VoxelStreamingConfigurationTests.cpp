#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Authoring/VoxelFineStreamingPolicy.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSourceViewRangeAuthorityTest,
	"WHFramework.Voxel.Configuration.SourceViewRangeAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSourceViewRangeAuthorityTest::RunTest(const FString& InParameters)
{
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -512;
	Manifest.Settings.MaxZ = 512;
	FVoxelViewSettings Settings;
	TArray<FVoxelStreamingSource> Sources;
	Sources.SetNum(2);
	for (auto& Source : Sources)
	{
		Source.Id = FGuid::NewGuid();
		Source.bInheritWorldView = false;
		Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual;
	}
	Sources[0].View.FineRadiusCells = 128;
	Sources[1].Center = FIntVector(2048, 0, 0);
	const FVoxelInterestSet Before = FVoxelInterestManager().Compute(Sources, Manifest, Settings);
	if (!TestTrue(TEXT("两独立Source完成正式需求规划"), Before.PlanningError.IsEmpty())) return false;
	const auto FirstBounds = Before.Sources.FindChecked(Sources[0].Id).GetCurrentWindow().Bounds;
	const auto SecondBounds = Before.Sources.FindChecked(Sources[1].Id).GetCurrentWindow().Bounds;
	TestEqual(TEXT("第一个Source使用自身128格水平半范围"), FirstBounds.Max.X - FirstBounds.Min.X, 256);
	TestEqual(TEXT("第二个Source使用自身256格水平半范围"), SecondBounds.Max.X - SecondBounds.Min.X, 512);
	const auto PreviousSource = Sources[0];
	Sources[0].View.FineRadiusCells = 192;
	const auto Change = FVoxelInterestRuntime::ClassifySourceChange(PreviousSource, Sources[0], Settings,
		Before.Sources.Find(Sources[0].Id));
	TestTrue(TEXT("仅半范围修改会触发真实驻留需求更新"), EnumHasAnyFlags(Change, EVoxelSourceChange::FineResidency));
	const FVoxelInterestSet After = FVoxelInterestManager().Compute(Sources, Manifest, Settings, &Before);
	if (!TestTrue(TEXT("修改后完成正式规划"), After.PlanningError.IsEmpty())) return false;
	const auto& First = After.Sources.FindChecked(Sources[0].Id);
	const auto& Second = After.Sources.FindChecked(Sources[1].Id);
	TestEqual(TEXT("第一个Source实际扩展至192格半范围"), First.GetCurrentWindow().Bounds.Max.X - First.GetCurrentWindow().Bounds.Min.X, 384);
	TestTrue(TEXT("第二个Source范围不受另一个Source修改影响"),
		Second.GetCurrentWindow().Bounds.Min == SecondBounds.Min && Second.GetCurrentWindow().Bounds.Max == SecondBounds.Max);
	const auto& Original = Before.Sources.FindChecked(Sources[0].Id);
	TestEqual(TEXT("旧冻结快照仍保留原范围"), Original.GetCurrentWindow().Bounds.Max.X - Original.GetCurrentWindow().Bounds.Min.X, 256);
	TestTrue(TEXT("扩大高精度范围不扩大F0交互核心"),
		First.F0CoreSections.Num() == Original.F0CoreSections.Num() && First.F0CoreSections.Includes(Original.F0CoreSections));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSourceViewRangeValidationTest,
	"WHFramework.Voxel.Configuration.SourceViewRangeValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSourceViewRangeValidationTest::RunTest(const FString& InParameters)
{
	const FVoxelFineStreamingPolicy Policy;
	FVoxelStreamingSourceView View;
	FString Error;
	if (!TestTrue(TEXT("统一默认范围满足完整安全约束"), Policy.Validate(View, Error))) return false;
	const auto Window = Policy.WindowSettings(View);
	TestEqual(TEXT("当前基线水平步移保持128格"), Window.ShiftStepCells, 128);
	TestEqual(TEXT("当前基线垂直步移保持32格"), Window.VerticalShiftStepCells, 32);
	TestEqual(TEXT("当前基线安全余量保持32格"), Window.GuardMarginCells, 32);
	View.FineRadiusCells = 80;
	TestFalse(TEXT("不对齐的旧半径不能被静默扩大或当作成功"), Policy.Validate(View, Error));
	TestFalse(TEXT("无效范围返回明确错误"), Error.IsEmpty());
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -512;
	Manifest.Settings.MaxZ = 512;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.View = View;
	Source.bInheritWorldView = false;
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual;
	const FVoxelInterestSet Rejected = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, {});
	TestFalse(TEXT("正式规划也拒绝无效Source范围"), Rejected.PlanningError.IsEmpty());
	return true;
}

#endif
