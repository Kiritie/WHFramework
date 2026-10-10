#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "PhysicsEngine/BodySetup.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCollisionWorldRetirementTest,
	"WHFramework.Voxel.FineRefactor.Physics.WorldGCBeforeNativeRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCollisionWorldRetirementTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UWorld::InitializationValues Initialization;
	Initialization.CreatePhysicsScene(true).ShouldSimulatePhysics(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Initialization);
	if (!TestNotNull(TEXT("Actual physics world is created"), World)) return false;
	TWeakObjectPtr<UWorld> WeakWorld(World);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	const auto Consumer = FVoxelGenerationConsumerKey::Section(88, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TUniquePtr<FVoxelTaskScheduler> Scheduler;
	TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> Retirement;
	ON_SCOPE_EXIT
	{
		if (Scheduler) Scheduler->StopAndJoin();
		if (Retirement) Retirement->Close();
		if (UWorld* Remaining = WeakWorld.Get())
		{
			GEngine->DestroyWorldContext(Remaining);
			Remaining->DestroyWorld(false);
		}
	};
	FString Error;
	if (!TestTrue(TEXT("Cook, task and retirement use the native original ledger"),
		FVoxelTerrainResources::Create({}, Consumer, Resources, Error) &&
		FVoxelTaskScheduler::Create(Resources.ToSharedRef(), Resources.ToSharedRef(), Consumer, Scheduler, Error) &&
		Scheduler->InitializeStatistics(Consumer.WorldEpoch))) return false;
	Retirement = FVoxelPhysicsRetirement::Create(*World->GetPhysicsScene(), Resources, Consumer);
	if (!TestTrue(TEXT("Native solver watcher is installed"), Retirement.IsValid())) return false;
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Installed collision has an actual World owner"), Owner)) return false;
	auto* Component = NewObject<UVoxelCollisionComponent>(Owner);
	Owner->SetRootComponent(Component);
	Component->RegisterComponent();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Physical;
	if (!TestTrue(TEXT("Original physical peak is granted before preparing geometry"),
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, 32768, Physical, 1))) return false;
	const FBox Box(FVector::ZeroVector, FVector(16));
	if (!TestTrue(TEXT("Complete box is prepared on its native physical lease"),
		Component->BeginPrepare(1, 25., Physical) && Component->AppendPreparedBoxes(MakeArrayView(&Box, 1)))) return false;
	const FVoxelTerrainServicePriority Priority{EVoxelTerrainServiceBand::P0, 0., 0., 0, Consumer.OwnerId};
	const double Deadline = FPlatformTime::Seconds() + 10.;
	bool bPrepared = false;
	do
	{
		const bool bAdmitted = Component->BeginDetachedCook(*Scheduler, Consumer, Physical, Priority,
			FPlatformTime::Seconds(), Retirement);
		Scheduler->Tick([](FVoxelTaskResult&&) {}, 8.);
		bPrepared = bAdmitted && !Component->IsDetachedCookRunning();
		if (!bPrepared) FPlatformProcess::Sleep(0.001f);
	} while (!bPrepared && FPlatformTime::Seconds() < Deadline);
	if (!TestTrue(TEXT("Actual detached worker completes and applies"), bPrepared && !Component->HasDetachedCookFailed()) ||
		!TestTrue(TEXT("Frozen native ticket is prepared"), Component->PrepareNativeRetirement())) return false;
	const auto Ticket = Component->CaptureNativeTicket();
	bool bInstalled = false;
	FPhysicsCommand::ExecuteWrite(World->GetPhysicsScene(), [&]() { bInstalled = Component->InstallPreparedPhysics(); });
	if (!TestTrue(TEXT("Worker geometry installs into the real solver"), bInstalled && Ticket && Ticket->Payload)) return false;
	World->Tick(LEVELTICK_All, 1.f / 60.f);
	World->GetPhysicsScene()->Flush();
	void* UserData = Ticket->Actor->GetGameThreadAPI().UserData();
	TestTrue(TEXT("Installed native actor resolves the live component BodyInstance"),
		FPhysicsUserData::Get<FBodyInstance>(UserData) == &Component->BodyInstance);
	TestTrue(TEXT("Native user-data address belongs to the retirement payload"), UserData != &Component->BodyInstance.PhysicsUserData);
	TWeakObjectPtr<UBodySetup> WeakSetup(Component->GetBodySetup());
	TestTrue(TEXT("Retained geometry has no component or World outer"), WeakSetup->GetOuter() == GetTransientPackage());
	TWeakObjectPtr<UVoxelCollisionComponent> WeakComponent(Component);
	Component->DestroyComponent();
	TestNull(TEXT("Removal detaches the retired actor from component memory"), FPhysicsUserData::Get<FBodyInstance>(UserData));
	TestTrue(TEXT("Scene removal still retains native geometry and the original payload"),
		Ticket->bRemovalIssued && !Ticket->bNativeRetired.Load() && Ticket->Payload.IsValid());
	Scheduler->StopAndJoin();
	Physical.Reset();
	Retirement->Close();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	World = nullptr;
	Owner = nullptr;
	Component = nullptr;
	CollectGarbage(RF_NoFlags, true);
	TestFalse(TEXT("Pending native retirement cannot keep the old collision component alive"), WeakComponent.IsValid());
	TestFalse(TEXT("Pending native retirement cannot keep the old World alive"), WeakWorld.IsValid());
	TestTrue(TEXT("Geometry remains independently pinned until the watcher consumes real solver destruction"), WeakSetup.IsValid() && Ticket->Payload.IsValid());
	FVoxelTerrainFrameContext Frame;
	if (!TestTrue(TEXT("Retirement continues through its original frame context"), Frame.Configure({}, Error))) return false;
	const double ReclaimDeadline = FPlatformTime::Seconds() + 5.;
	uint64 FrameNumber = 0;
	do
	{
		Frame.Begin(Consumer.WorldEpoch, ++FrameNumber, FPlatformTime::Seconds(), {});
		Retirement->Advance(Frame);
		Frame.End(FPlatformTime::Seconds());
		if (Ticket->Payload) FPlatformProcess::Sleep(0.001f);
	} while (Ticket->Payload && FPlatformTime::Seconds() < ReclaimDeadline);
	TestTrue(TEXT("Actual solver teardown retires the original ticket and releases its payload"),
		Ticket->bNativeRetired.Load() && !Ticket->bRegistered && !Ticket->Payload);
	return true;
}

#endif
