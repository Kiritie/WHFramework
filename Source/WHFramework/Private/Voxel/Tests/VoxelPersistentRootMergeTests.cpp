#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelPersistentRootMerge.h"

namespace
{
	struct FMergeNode
	{
		TSharedPtr<const FMergeNode, ESPMode::ThreadSafe> Children[2];
		int32 Value = INDEX_NONE;
	};
	using FMergeTestNodePtr = TSharedPtr<const FMergeNode, ESPMode::ThreadSafe>;
	FMergeTestNodePtr MakeMergeTestLeaf(const int32 Value)
	{
		auto Node = MakeShared<FMergeNode, ESPMode::ThreadSafe>();
		Node->Value = Value;
		return Node;
	}
	FMergeTestNodePtr MakeMergeTestBranch(const FMergeTestNodePtr& Left, const FMergeTestNodePtr& Right)
	{
		auto Node = MakeShared<FMergeNode, ESPMode::ThreadSafe>();
		Node->Children[0] = Left;
		Node->Children[1] = Right;
		return Node;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPersistentRootMergeTest,
	"WHFramework.Voxel.Rendering.PersistentRootLocalMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPersistentRootMergeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FMergeTestNodePtr Base = MakeMergeTestBranch(MakeMergeTestBranch(MakeMergeTestLeaf(0), MakeMergeTestLeaf(0)), MakeMergeTestBranch(MakeMergeTestLeaf(0), MakeMergeTestLeaf(0)));
	const FMergeTestNodePtr Current = MakeMergeTestBranch(MakeMergeTestBranch(MakeMergeTestLeaf(7), Base->Children[0]->Children[1]), Base->Children[1]);
	const FMergeTestNodePtr Prepared = MakeMergeTestBranch(Base->Children[0], MakeMergeTestBranch(Base->Children[1]->Children[0], MakeMergeTestLeaf(9)));
	TVoxelPersistentRootMerge<FMergeNode, 2, 3> Merge;
	Merge.Begin(Base, Current, Prepared);
	int32 Allocations = 0;
	bool bCapacity = false;
	const auto Advance = [&]
	{
		return Merge.Step([](const auto& A, const auto& B) { return A == B; },
			[](const auto& B, const auto& C, const auto& P)
			{
				const auto& Any = P ? P : C ? C : B;
				return Any && Any->Value != INDEX_NONE;
			},
			[](const auto&, const auto&, const auto&, auto&) { return EVoxelPersistentMergeProgress::Conflict; },
			[&](const auto&, const auto&, const auto&, auto& Out)
			{
				if (!bCapacity) return false;
				++Allocations;
				Out = MakeShared<FMergeNode, ESPMode::ThreadSafe>();
				return true;
			}, [](auto&) {});
	};
	TestTrue(TEXT("未授予容量时保持合并游标"), Advance() == EVoxelPersistentMergeProgress::CapacityBlocked);
	TestTrue(TEXT("容量不足不会先分配节点"), Allocations == 0);
	bCapacity = true;
	EVoxelPersistentMergeProgress Progress = EVoxelPersistentMergeProgress::Working;
	int32 Steps = 0;
	while (Progress == EVoxelPersistentMergeProgress::Working && Steps++ < 64) Progress = Advance();
	if (!TestTrue(TEXT("不相交增量分步合并完成"), Progress == EVoxelPersistentMergeProgress::Complete)) return false;
	const FMergeTestNodePtr Result = Merge.GetResult();
	TestTrue(TEXT("已提交近景修改不会被旧基线覆盖"), Result->Children[0] == Current->Children[0]);
	TestTrue(TEXT("远景准备结果继续共享已准备子树"), Result->Children[1] == Prepared->Children[1]);
	TestEqual(TEXT("不相交增量只复制公共根"), Allocations, 1);
	TestTrue(TEXT("Current 和候选输入均保持不变"), Base->Children[0]->Children[0]->Value == 0 && Current->Children[0]->Children[0]->Value == 7);
	Merge.Reset();
	const FMergeTestNodePtr Conflict = MakeMergeTestBranch(MakeMergeTestBranch(MakeMergeTestLeaf(11), Base->Children[0]->Children[1]), Base->Children[1]);
	Merge.Begin(Base, Current, Conflict);
	Progress = EVoxelPersistentMergeProgress::Working;
	Steps = 0;
	while (Progress == EVoxelPersistentMergeProgress::Working && Steps++ < 64) Progress = Advance();
	TestTrue(TEXT("同一叶子的不同修改必须局部重试"), Progress == EVoxelPersistentMergeProgress::Conflict);
	TestEqual(TEXT("冲突不能回写 Current"), Current->Children[0]->Children[0]->Value, 7);
	Merge.Reset();
	const FMergeTestNodePtr SameBranch = MakeMergeTestBranch(MakeMergeTestBranch(Base->Children[0]->Children[0], MakeMergeTestLeaf(13)), Base->Children[1]);
	Merge.Begin(Base, Current, SameBranch);
	Allocations = 0;
	Progress = EVoxelPersistentMergeProgress::Working;
	Steps = 0;
	while (Progress == EVoxelPersistentMergeProgress::Working && Steps++ < 64) Progress = Advance();
	if (!TestTrue(TEXT("同一粗根内不同叶子允许合并"), Progress == EVoxelPersistentMergeProgress::Complete)) return false;
	TestTrue(TEXT("两个局部修改同时保留"), Merge.GetResult()->Children[0]->Children[0]->Value == 7 &&
		Merge.GetResult()->Children[0]->Children[1]->Value == 13);
	TestTrue(TEXT("未受影响的远处子树继续共享"), Merge.GetResult()->Children[1] == Base->Children[1]);
	TestEqual(TEXT("只复制两条变更路径的公共祖先"), Allocations, 2);
	return true;
}

#endif
