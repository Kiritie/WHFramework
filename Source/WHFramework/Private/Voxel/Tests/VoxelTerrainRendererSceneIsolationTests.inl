#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainRendererSceneIsolationTest,
	"WHFramework.Voxel.Rendering.SceneReceiptIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainRendererSceneIsolationTest::RunTest(const FString&)
{
	UWorld* Worlds[2] = {};
	TStrongObjectPtr<UVoxelModule> Modules[2];
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Terrain[2], Runtime[2];
	TUniquePtr<FVoxelTaskScheduler> Schedulers[2];
	TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> States[2];
	TSharedPtr<FVoxelTerrainViewExtension, ESPMode::ThreadSafe> Extensions[2];
	ON_SCOPE_EXIT
	{
		Extensions[0].Reset();
		Extensions[1].Reset();
		FlushRenderingCommands();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			States[Index].Reset();
			Schedulers[Index].Reset();
			Modules[Index].Reset();
			if (Worlds[Index])
			{
				GEngine->DestroyWorldContext(Worlds[Index]);
				Worlds[Index]->DestroyWorld(false);
			}
		}
	};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Worlds[Index] = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		if (!TestNotNull(TEXT("创建独立渲染场景"), Worlds[Index])) return false;
		GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(Worlds[Index]);
		Modules[Index].Reset(NewObject<UVoxelModule>(Worlds[Index]));
		const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
		FString Error;
		if (!TestTrue(TEXT("创建独立资源与原生调度器"),
			FVoxelTerrainResources::Create({}, Consumer, Terrain[Index], Error) &&
			FVoxelTerrainResources::Create({}, Consumer, Runtime[Index], Error) &&
			FVoxelTaskScheduler::Create(Terrain[Index].ToSharedRef(), Runtime[Index].ToSharedRef(), Consumer, Schedulers[Index], Error))) return false;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!TestTrue(TEXT("取得实际渲染状态输入租约"), Terrain[Index]->Acquire(Consumer,
			EVoxelTerrainResourcePool::Input, VoxelRendererStateInputBytes(true), Lease))) return false;
		States[Index] = MakeShared<FVoxelTerrainRendererState, ESPMode::ThreadSafe>(
			*Modules[Index], *Schedulers[Index], 1, Consumer, Lease);
		Extensions[Index] = FSceneViewExtensions::NewExtension<FVoxelTerrainViewExtension>(
			States[Index].ToSharedRef(), Worlds[Index]->Scene);
	}
	TestTrue(TEXT("相同epoch仍拥有不同场景"), Worlds[0]->Scene != Worlds[1]->Scene);
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		const int32 Owner = Scenario == 2 ? 0 : Scenario;
		FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(
			nullptr, Worlds[Owner]->Scene, FEngineShowFlags(ESFIM_Game)));
		auto Snapshot = MakeShared<FRenderSnapshot, ESPMode::ThreadSafe>();
		Snapshot->WorldEpoch = 1;
		Snapshot->RendererOwner = States[Scenario == 2 ? 1 : Owner]->OwnerId;
		Snapshot->Serial = 1;
		FFrameBinding* Binding = Family.GetOrCreateExtentionData<FFrameBinding>();
		Binding->Snapshot = Snapshot;
		Binding->bProxySubmitted.Store(true);
		const auto First = Extensions[0];
		const auto Second = Extensions[1];
		ENQUEUE_RENDER_COMMAND(VoxelTerrainSceneReceiptIsolation)(
			[First, Second, &Family](FRHICommandListImmediate& Commands)
			{
				FRDGBuilder Graph(Commands);
				First->PostRenderViewFamily_RenderThread(Graph, Family);
				Second->PostRenderViewFamily_RenderThread(Graph, Family);
				Graph.Execute();
			});
		FlushRenderingCommands();
		if (Scenario == 2)
		{
			TestTrue(TEXT("同场景的错误渲染所有者也不能提交"), States[0]->Submissions.IsEmpty() && States[1]->Submissions.IsEmpty());
			continue;
		}
		TestTrue(TEXT("其他世界未收到本场景回执"), States[1 - Owner]->Submissions.IsEmpty());
		FSnapshotPtr Received;
		FFrameSubmissionData Submission;
		if (!TestTrue(TEXT("所属世界收到真实RT回执"), States[Owner]->Submissions.TryConsume(Received, Submission))) return false;
		TestTrue(TEXT("回执保留原始只读帧身份"), Received == Snapshot);
		TestTrue(TEXT("原生GPU提交及首次回执标记保留"), Submission.GpuMask != 0 && Submission.bHasReceipts);
		TestTrue(TEXT("所属队列只消费一次"), States[Owner]->Submissions.IsEmpty());
	}
	return true;
}

#endif
