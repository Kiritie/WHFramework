#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineGlobals.h"
#include "HAL/PlatformProcess.h"
#include "Main/MainModule.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Navigation/VoxelNavigationPresenter.h"
#include "Voxel/Navigation/VoxelNavigationSurfaceComponent.h"
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Tests/VoxelNavigationTestTypes.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/Data/VoxelData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationResidencyTest,
	"WHFramework.Voxel.Navigation.SimulationCollisionAndDataEviction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationResidencyTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Navigation fixture world is created"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AMainModule* Owner = World->SpawnActor<AMainModule>();
	UVoxelModule* Module = NewObject<UVoxelModule>(Owner);
	ON_SCOPE_EXIT
	{
		FString CleanupError;
		Module->StopWorld(true, CleanupError);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	if (!Owner->GetRootComponent())
	{
		USceneComponent* Root = NewObject<USceneComponent>(Owner);
		Owner->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	Module->bEnableProjectSceneSimulation = true;
	Module->SceneRegionClass = UVoxelNavigationTestSceneRegion::StaticClass();
	Module->Epoch = 11;
	Module->Manifest.BlockSizeCentimeters = 25;
	Module->NavigationClearanceCentimeters = 175.;
	UVoxelData* Support = NewObject<UVoxelData>();
	Support->BlockName = TEXT("test:navigation_support");
	Support->DropCount = 0;
	FString Error;
	if (!TestTrue(TEXT("Fixture uses the real block registry"), Module->Registry.Build({Support}, false, Error)))
	{
		AddError(Error);
		return false;
	}
	const uint16 SupportType = Module->Registry.GetSnapshot()->Find(Support->BlockName)->TypeId;
	const auto Shapes = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
	Shapes->BuildDefaults();
	Module->Shapes = Shapes;
	Module->Generator = VoxelTest::MakeGenerator();
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(Module->Epoch, true, Module->Registry.GetSnapshot().ToSharedRef(), Module->Generator.ToSharedRef());
	Module->Scheduler = MakeUnique<FVoxelTaskScheduler>();
	Module->CollisionPresenter = MakeUnique<FVoxelCollisionPresenter>(*Module, *Module->Scheduler, Module->Epoch);
	Module->NavigationPresenter = MakeUnique<FVoxelNavigationPresenter>(*Module, *Module->Scheduler);
	Module->ResidencyManager = MakeUnique<FVoxelResidencyManager>(*Module->Runtime, *Module->Scheduler,
		[Module](const FIntVector& Section) { Module->OnSectionEvicting(Section); });
	Module->ResidencyManager->SetEvictGraceFrames(0);
	const FIntVector Lower(0, 0, 0);
	const FIntVector Above(0, 0, 1);
	const FIntVector Distant(3, 0, 0);
	auto PublishSection = [&](const FIntVector& Key, const int32 SupportHeight)
	{
		FVoxelSection* Section = Module->Runtime->FindOrAllocate(Key, 0);
		TArray<FVoxelBlockState> Blocks;
		Blocks.Init(FVoxelBlockState(), 4096);
		if (SupportHeight >= 0) Blocks[4 + 16 * (4 + 16 * SupportHeight)] = FVoxelBlockState(SupportType, 0);
		return Section && Module->Runtime->PublishBase(Key, Section->Stamp, MoveTemp(Blocks), Error) &&
			Module->Runtime->PublishFinal(Key, 0, {}, {}, Error);
	};
	if (!TestTrue(TEXT("Known support and upper clearance publish through the runtime"),
		PublishSection(Lower, 15) && PublishSection(Above, -1) && PublishSection(Distant, 0)))
	{
		AddError(Error);
		return false;
	}
	Module->CurrentInterest.Exact.Add(Above, FVoxelExactDemand());
	for (const FIntVector& Key : {Lower, Distant})
	{
		FVoxelExactDemand& Demand = Module->CurrentInterest.Exact.Add(Key);
		Demand.bSimulation = true;
		TestFalse(TEXT("Simulation cannot activate before actual collision publication"), Module->ActivateSimulationSection(Key));
		Demand.bSimulation = false;
		Demand.bCollision = true;
	}
	auto FindSurface = [&](const FIntVector& Key) -> UVoxelNavigationSurfaceComponent*
	{
		TInlineComponentArray<UVoxelNavigationSurfaceComponent*> Components(Owner);
		for (UVoxelNavigationSurfaceComponent* Component : Components)
		{
			if (Component->GetComponentLocation().Equals(FVector(Key * 16) * Module->BlockSize())) return Component;
		}
		return nullptr;
	};
	auto Pump = [&](TFunctionRef<bool()> Finished)
	{
		const double Deadline = FPlatformTime::Seconds() + 10.;
		do
		{
			Module->CollisionPresenter->Tick(Module->CurrentInterest.Exact, 1);
			Module->NavigationPresenter->Tick();
			Module->Scheduler->Tick([Module](FVoxelTaskResult&& Result)
			{
				Module->CollisionPresenter->OnTask(MoveTemp(Result));
			}, 8.);
			FPlatformProcess::Sleep(0.001f);
		}
		while (!Finished() && FPlatformTime::Seconds() < Deadline);
		return Finished();
	};
	if (!TestTrue(TEXT("Real collision jobs publish their physics components"), Pump([&]()
	{
		return Module->IsCollisionReady(Lower) && Module->IsCollisionReady(Distant);
	}))) return false;
	TestNull(TEXT("Collision/data residency alone creates no navigation surface"), FindSurface(Lower));
	TestFalse(TEXT("Collision readiness without Simulation cannot activate a scene region"), Module->ActivateSimulationSection(Lower));
	for (const FIntVector& Key : {Lower, Distant}) Module->CurrentInterest.Exact.FindChecked(Key).bSimulation = true;
	Module->RefreshSimulationResidency();
	if (!TestTrue(TEXT("Navigation exports actual geometry after Simulation activation"), Pump([&]()
	{
		return FindSurface(Lower) && !FindSurface(Lower)->GetSurface().Indices.IsEmpty() &&
			FindSurface(Distant) && !FindSurface(Distant)->GetSurface().Indices.IsEmpty();
	}))) return false;
	TestTrue(TEXT("Lower section is simulation resident"), Module->IsSimulationResident(Lower));
	TestFalse(TEXT("Upper clearance section remains data-only"), Module->IsSimulationResident(Above));
	TestEqual(TEXT("One support face is exported across the section boundary"), FindSurface(Lower)->GetSurface().Indices.Num(), 6);

	Module->CurrentInterest.Exact.Remove(Above);
	const uint64 PreviousFrame = GFrameCounter;
	GFrameCounter += 15;
	Module->ResidencyManager->Tick(Module->CurrentInterest.Exact, 2, 0.);
	GFrameCounter = PreviousFrame;
	TestNull(TEXT("The actual residency owner evicts unneeded upper data"), Module->Runtime->FindSection(Above));
	TestTrue(TEXT("Eviction immediately withdraws the dependent lower navigation face"), FindSurface(Lower)->GetSurface().Indices.IsEmpty());
	TestEqual(TEXT("Distant navigation survives local upper-data eviction"), FindSurface(Distant)->GetSurface().Indices.Num(), 6);
	TestTrue(TEXT("The lower data and Simulation owner remain resident"), Module->Runtime->FindSection(Lower) && Module->IsSimulationResident(Lower));
	TestTrue(TEXT("Conservative navigation rebuild completes without upper data"), Pump([&]()
	{
		return Module->Scheduler->GetDiagnostics().Pending == 0 && Module->Scheduler->ActiveCount() == 0;
	}));
	TestTrue(TEXT("Unknown upper clearance cannot resurrect the old face"), FindSurface(Lower)->GetSurface().Indices.IsEmpty());

	TestTrue(TEXT("Upper data can be republished"), PublishSection(Above, -1));
	Module->NavigationPresenter->InvalidateSection(Above);
	if (!TestTrue(TEXT("Known upper clearance restores the lower face"), Pump([&]()
	{
		return !FindSurface(Lower)->GetSurface().Indices.IsEmpty();
	}))) return false;
	FVoxelCellEdit Ceiling;
	Ceiling.Position = FIntVector(4, 4, 19);
	Ceiling.Value = FVoxelBlockState(SupportType, 0);
	FVoxelPreparedEdit Prepared;
	FVoxelEditBatch Batch;
	if (!TestTrue(TEXT("Overhead edit commits through the runtime"), Module->Runtime->PrepareEdit({Ceiling}, {}, Prepared, Error) &&
		Module->Runtime->CommitPreparedEdit(MoveTemp(Prepared), Batch, Error))) return false;
	Module->PublishProjectEdit(Batch);
	TestTrue(TEXT("Committed overhead edit immediately invalidates dependent navigation"), FindSurface(Lower)->GetSurface().Indices.IsEmpty());
	TestEqual(TEXT("Committed overhead edit leaves distant navigation intact"), FindSurface(Distant)->GetSurface().Indices.Num(), 6);

	Module->NavigationPresenter->Tick();
	Module->CurrentInterest.Exact.FindChecked(Lower).bSimulation = false;
	Module->RefreshSimulationResidency();
	TestFalse(TEXT("Removing only Simulation retires the scene residency immediately"), Module->IsSimulationResident(Lower));
	TestNull(TEXT("Removing only Simulation destroys its navigation component"), FindSurface(Lower));
	TestNotNull(TEXT("Simulation removal retains demanded exact data"), Module->Runtime->FindSection(Lower));
	TestTrue(TEXT("Already queued navigation work drains"), Pump([&]()
	{
		return Module->Scheduler->GetDiagnostics().Pending == 0 && Module->Scheduler->ActiveCount() == 0;
	}));
	TestNull(TEXT("A late result cannot resurrect retired navigation"), FindSurface(Lower));
	TestEqual(TEXT("An independent Simulation island keeps its exported face"), FindSurface(Distant)->GetSurface().Indices.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationDoorClearanceTest,
	"WHFramework.Voxel.Navigation.ObjectDoorClearanceTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationDoorClearanceTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRegistrySnapshot Registry;
	Registry.Definitions.SetNum(3);
	Registry.Definitions[1].TypeId = 1;
	Registry.Definitions[1].bSolid = true;
	Registry.Definitions[2].TypeId = 2;
	Registry.Definitions[2].bSolid = true;
	Registry.Definitions[2].Shape = EVoxelShapeKind::ObjectPart;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelSectionSnapshot Section;
	Section.Blocks.Init(0, 4096);
	Section.Blocks[4 + 16 * 4] = FVoxelBlockState(1, 0).Pack();
	const int32 DoorCell = 4 + 16 * (4 + 16);
	Section.Blocks[DoorCell] = FVoxelBlockState(2, 0).Pack();
	FVoxelNavigationSurface Surface;
	TestTrue(TEXT("Closed-door geometry builds"), FVoxelNavigationSurfaceBuilder::Build(Section, nullptr, Registry, Shapes, 25., 175., Surface));
	TestTrue(TEXT("Closed object door removes floor clearance"), Surface.Indices.IsEmpty());
	Section.Blocks[DoorCell] = FVoxelBlockState(2, VoxelState::OpenMask).Pack();
	TestTrue(TEXT("Open-door geometry rebuilds"), FVoxelNavigationSurfaceBuilder::Build(Section, nullptr, Registry, Shapes, 25., 175., Surface));
	TestEqual(TEXT("Opening the actual collision state restores the floor face"), Surface.Indices.Num(), 6);
	Section.Blocks[DoorCell] = FVoxelBlockState(2, 0).Pack();
	FVoxelNavigationSurfaceBuilder::Build(Section, nullptr, Registry, Shapes, 25., 175., Surface);
	TestTrue(TEXT("Closing again replaces rather than appends to old geometry"), Surface.Indices.IsEmpty());
	return true;
}

#endif
