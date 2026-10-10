#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Main/MainModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PrimitiveSceneProxy.h"
#include "RenderingThread.h"
#include "UObject/UObjectIterator.h"
#include "Voxel/Components/VoxelTerrainPrimitiveComponent.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainRendererWorldCleanupTest,
	"WHFramework.Voxel.Rendering.RetirementReleasesWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainRendererWorldCleanupTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Creates an independent rendering world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
	AMainModule* Owner = World->SpawnActor<AMainModule>();
	UVoxelModule* Module = NewObject<UVoxelModule>(Owner);
	Module->Epoch = 71;
	FString Error;
	const auto Control = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> TerrainResources, RuntimeResources;
	TUniquePtr<FVoxelTaskScheduler> SchedulerOwner;
	if (!TestTrue(TEXT("Creates the original production control ledgers and scheduler"),
		FVoxelTerrainResources::Create({}, Control, TerrainResources, Error) &&
		FVoxelTerrainResources::Create({}, Control, RuntimeResources, Error) &&
		FVoxelTaskScheduler::Create(TerrainResources.ToSharedRef(), RuntimeResources.ToSharedRef(), Control, SchedulerOwner, Error))) return false;
	auto& Scheduler = *SchedulerOwner;
	if (!TestTrue(TEXT("Initializes the actual scheduler control"), Scheduler.InitializeStatistics(71))) return false;
	FVoxelTerrainRenderer Renderer(*Module, Scheduler, 71);
	TWeakObjectPtr<UVoxelTerrainPrimitiveComponent> Component;
	for (TObjectIterator<UVoxelTerrainPrimitiveComponent> It; It; ++It)
	{
		if (It->GetWorld() == World) Component = *It;
	}
	if (!TestTrue(TEXT("Renderer creates and registers its native primitive"), Component.IsValid())) return false;
	TestTrue(TEXT("Retirement material holder has a world-independent outer"), Component->GetOuter() == GetTransientPackage());
	TWeakObjectPtr<UMaterialInstanceDynamic> HeldMaterial;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* Material = UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), GetTransientPackage());
		HeldMaterial = Material;
		EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::None;
		if (!TestEqual(TEXT("Original native material append retains the expected UE slot"),
			Component->RetainMaterial(Material, &Admission), Index)) return false;
		TestEqual(TEXT("Original material backing is granted"), Admission, EVoxelTerrainResourceAdmission::Granted);
		TestEqual(TEXT("UE material slot count includes all retained materials"), Component->GetNumMaterials(), Index + 1);
		TArray<UMaterialInterface*> Used;
		Component->GetUsedMaterials(Used, true);
		TestTrue(TEXT("Inherited UE material enumeration declares the actual retained material"), Used.Contains(Material));
		World->SendAllEndOfFrameUpdates();
		FlushRenderingCommands();
		auto* Proxy = Component->GetSceneProxy();
		if (!TestNotNull(TEXT("UE end-of-frame material update recreates the real native proxy"), Proxy)) return false;
		TAtomic<bool> Verified(false);
		auto* RenderMaterial = Material->GetRenderProxy();
		ENQUEUE_RENDER_COMMAND(VoxelVerifyRetainedMaterial)([Proxy, RenderMaterial, &Verified](FRHICommandListImmediate&)
		{
			Verified.Store(Proxy->VerifyUsedMaterial(RenderMaterial));
		});
		FlushRenderingCommands();
		TestTrue(TEXT("Actual UE render-thread verification accepts materials appended after proxy creation"), Verified.Load());
		const uint64 BeforeDuplicate = TerrainResources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
		TestEqual(TEXT("Duplicate material retains its original slot"), Component->RetainMaterial(Material, &Admission), Index);
		TestFalse(TEXT("Duplicate does not request another proxy recreation"), Component->IsRenderStateDirty());
		TestEqual(TEXT("Duplicate material requires no new native Input grant"),
			TerrainResources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], BeforeDuplicate);
		TestEqual(TEXT("Temporary original proxy material Working backing is released"),
			TerrainResources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	}
	auto PendingRenderReference = Component->TerrainState;
	Renderer.Reset();
	TestFalse(TEXT("Shutdown unregisters the primitive before world collection"), Component->IsRegistered());
	TestNull(TEXT("Unregistered primitive no longer belongs to the ended world"), Component->GetWorld());
	const TWeakObjectPtr<UWorld> EndedWorld = World;
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	World = nullptr;
	FlushRenderingCommands();
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("An outstanding native render reference cannot retain the ended world"), EndedWorld.IsValid());
	TestTrue(TEXT("Material holder survives until its native retirement references finish"), Component.IsValid());
	TestTrue(TEXT("Retained material survives GC while original retirement references remain"), HeldMaterial.IsValid());
	PendingRenderReference.Reset();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	FlushRenderingCommands();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Final retirement releases the material holder"), Component.IsValid());
	TestFalse(TEXT("Final retirement also releases the actual retained material"), HeldMaterial.IsValid());
	return true;
}

#endif
