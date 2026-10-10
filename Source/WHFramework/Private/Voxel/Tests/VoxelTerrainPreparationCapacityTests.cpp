#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Main/MainModule.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelGameThreadContinuation.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainPreparationInputTest,
	"WHFramework.Voxel.Rendering.NativePreparationBoundedInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVoxelTerrainPreparationFrames,
	TSharedRef<FVoxelGameThreadContinuation>, Work);

bool FVoxelTerrainPreparationFrames::Update()
{
	Work->Resume();
	return Work->IsComplete();
}

bool FVoxelTerrainPreparationInputTest::RunTest(const FString&)
{
	struct FScenario
	{
		static FVoxelGameThreadContinuation Execute(FVoxelTerrainPreparationInputTest* Test)
		{
			if (!Test->TestTrue(TEXT("Native capacity regression requires the actual renderer"), FApp::CanEverRender())) co_return;
			UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!Test->TestNotNull(TEXT("Creates an independent native rendering world"), World)) co_return;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			ON_SCOPE_EXIT
			{
				FlushRenderingCommands();
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			};
			auto* Owner = World->SpawnActor<AMainModule>();
			auto* Module = NewObject<UVoxelModule>(Owner);
			TStrongObjectPtr<UVoxelModule> ModulePin(Module);
			Module->Epoch = 71;
			FString Error;
			if (!Test->TestTrue(TEXT("Builds the original frozen registry"), Module->Registry.Build({}, false, Error))) co_return;
			auto Shapes = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
			Shapes->BuildDefaults();
			Module->Shapes = Shapes;
			Module->Generator = VoxelTest::MakeGenerator();
			Module->GenerationConfig = VoxelTest::MakeGenerationConfig();
			const auto Control = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, {}, 1);
			TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources, RuntimeResources;
			TUniquePtr<FVoxelTaskScheduler> SchedulerOwner;
			if (!Test->TestTrue(TEXT("Creates the original production ledgers and scheduler"),
				FVoxelTerrainResources::Create({}, Control, Resources, Error) &&
				FVoxelTerrainResources::Create({}, Control, RuntimeResources, Error) &&
				FVoxelTaskScheduler::Create(Resources.ToSharedRef(), RuntimeResources.ToSharedRef(), Control, SchedulerOwner, Error))) co_return;
			auto& Scheduler = *SchedulerOwner;
			if (!Test->TestTrue(TEXT("Initializes actual work accounting"), Scheduler.InitializeStatistics(71))) co_return;
			Module->Runtime = MakeUnique<FVoxelWorldRuntime>(71, true, Module->GetRegistry().ToSharedRef(),
				Module->Generator.ToSharedRef(), RuntimeResources.ToSharedRef());
			FVoxelTerrainRenderer Renderer(*Module, Scheduler, 71);
			ON_SCOPE_EXIT { Scheduler.StopAndJoin(); Renderer.Reset(); };
			auto* CaptureActor = World->SpawnActor<AActor>();
			auto* Capture = NewObject<USceneCaptureComponent2D>(CaptureActor);
			CaptureActor->SetRootComponent(Capture);
			CaptureActor->AddInstanceComponent(Capture);
			Capture->bCaptureEveryFrame = Capture->bCaptureOnMovement = false;
			Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Capture->TextureTarget = NewObject<UTextureRenderTarget2D>(Capture);
			Capture->TextureTarget->InitAutoFormat(64, 64);
			Capture->RegisterComponent();
			CaptureActor->SetActorLocation(FVector(-5000, -5000, 6000));
			CaptureActor->SetActorRotation(FRotator(-35, 45, 0));
			World->SendAllEndOfFrameUpdates();
			FlushRenderingCommands();

			TArray<TSharedPtr<const FVoxelTerrainRenderAtom, ESPMode::ThreadSafe>, TInlineAllocator<8>> Atoms;
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const FIntVector Section(Index * 3, 0, 0);
				const auto Consumer = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, Section, 1);
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GeometryLease, BoundaryLease, AtomLease;
				const uint64 BoundaryBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineBoundarySnapshot>() +
					VoxelBoundedAllocation::ArrayBytes<FVoxelFineAtomBoundary>(1) + 24 * VoxelBoundedAllocation::ArrayBytes<uint32>(1);
				const uint64 AtomBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderAtom>() +
					VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>>(1);
				if (!Test->TestTrue(TEXT("Frozen bodies use original grants before allocation"),
					Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged,
						VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineShardGeometry>(), GeometryLease) &&
					Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, BoundaryBytes, BoundaryLease) &&
					Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, AtomBytes, AtomLease))) co_return;
				auto Geometry = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineShardGeometry>(GeometryLease);
				Geometry->MetadataLease = GeometryLease;
				Geometry->GeometryIdentity = FGuid::NewGuid();
				Geometry->bKnownEmpty = Geometry->bCoverageValidated = true;
				auto& Input = Geometry->Attempt.Input;
				Input.WorldEpoch = 71; Input.Incarnation = 1; Input.GeneratorIdentity = 1;
				Input.RegistryRevision = Module->GetRegistry()->Hash; Input.ShapeRevision = Shapes->GetContentHash();
				Input.DataRevision = Input.OverlayRevision = Input.MeshRevision = 1;
				Input.Shard = {Section * 16, 16};
				auto Boundary = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineBoundarySnapshot>(BoundaryLease);
				Boundary->ResourceLease = BoundaryLease;
				Boundary->Input = Input;
				Boundary->Atoms.Reserve(1);
				auto& BoundaryAtom = Boundary->Atoms.AddDefaulted_GetRef();
				BoundaryAtom.Section = Section;
				for (int32 Face = 0; Face < 6; ++Face)
				{
					for (auto* Values : {&Boundary->Owned[Face], &Boundary->Halo[Face],
						&BoundaryAtom.Owned[Face], &BoundaryAtom.Halo[Face]})
					{
						Values->Reserve(1);
						Values->Add(0);
					}
				}
				if (!Test->TestTrue(TEXT("Frozen all-air boundary validates its actual funded arrays"), Boundary->Validate())) co_return;
				Geometry->Boundary = Boundary;
				auto Atom = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainRenderAtom>(AtomLease);
				Atom->MetadataLease = AtomLease;
				Atom->Section = Section;
				Atom->FineGeometry = Geometry;
				Atom->Boundaries.Reserve(1);
				Atom->Boundaries.Add(Boundary);
				Atom->Evidence.Owner = {EVoxelFineOwnerKind::F1Plus, FGuid::NewGuid(), Input.Incarnation};
				auto& Content = Atom->Evidence.Content;
				Content.WorldEpoch = Input.WorldEpoch; Content.GeneratorIdentity = Input.GeneratorIdentity;
				Content.RegistryRevision = Input.RegistryRevision; Content.ShapeRevision = Input.ShapeRevision;
				Content.DataRevision = Input.DataRevision; Content.OverlayRevision = Input.OverlayRevision;
				Content.MeshRevision = Input.MeshRevision; Content.GeometryIdentity = Geometry->GeometryIdentity;
				Content.SampleStepCells = 1;
				Atom->Evidence.bGeometryReady = Atom->Evidence.bKnownEmpty = Atom->Evidence.bCanStage = true;
				Atom->Evidence.GeometryLease = Geometry;
				if (!Test->TestTrue(TEXT("Original empty Fine Atom has matching native inputs"), Atom->IsResourceReady())) co_return;
				Atoms.Add(MoveTemp(Atom));
			}
			int32 Submitted = 0;
			FGuid Transaction;
			EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::None;
			if (!Test->TestTrue(TEXT("Eight real Fine Atoms enter one original transaction"),
				Renderer.PrepareAtomTransaction(Atoms, 1, false,
					[](const FIntVector&, const FVoxelFineOwnerEvidence&) { return true; },
					[](const FIntVector&, const FVoxelFineOwnerEvidence&) { return true; },
					[&Submitted] { ++Submitted; }, Transaction, {}, -1.0, &Admission))) co_return;
			Test->TestEqual(TEXT("Original transaction preparation is granted"), Admission, EVoxelTerrainResourceAdmission::Granted);
			Test->TestFalse(TEXT("Preparing the transaction does not manufacture a presented root"), Renderer.CapturePresentedSnapshot().IsValid());
			const auto BeforePressure = Resources->Snapshot();
			const uint64 HandleBytes = BeforePressure.LeaseHandleBytes / uint64(BeforePressure.LeaseCount);
			constexpr uint64 RemainingBytes = 256 * 1024;
			// Handoff 临时持有者在真实后台退休；给其退款留出1 KiB，验证原生输入上界而非瞬时等值。
			constexpr uint64 TargetRemainingBytes = RemainingBytes - 1024;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
			const auto External = FVoxelGenerationConsumerKey::Section(71, FGuid::NewGuid(), 1, {}, 1);
			if (!Test->TestEqual(TEXT("Actual external Input pressure leaves only 256 KiB"), Resources->TryAcquire(External,
				EVoxelTerrainResourcePool::Input, BeforePressure.Limits.InputBytes - BeforePressure.Used[0] - HandleBytes - TargetRemainingBytes,
				Pressure), EVoxelTerrainResourceAdmission::Granted)) co_return;
			const auto PressureState = Resources->Snapshot();
			if (!Test->TestTrue(TEXT("Original ledger has no more than 256 KiB of remaining Input"),
				PressureState.Limits.InputBytes - PressureState.Used[0] <= RemainingBytes)) co_return;
			const uint64 PressureSerial = Pressure->Serial();
			const uint64 PressureBytes = Pressure->Bytes();
			const double Deadline = FPlatformTime::Seconds() + 10.0;
			while (Submitted == 0 && FPlatformTime::Seconds() < Deadline)
			{
				auto& Frame = Module->GetTerrainFrameContext();
				Frame.Begin(71, GFrameCounter, FPlatformTime::Seconds(), {});
				Scheduler.Tick([](FVoxelTaskResult&&) {});
				Renderer.AdvanceFramePublication();
				World->SendAllEndOfFrameUpdates();
				Capture->CaptureScene();
				FlushRenderingCommands();
				Renderer.PollRenderReceipts();
				Renderer.AdvanceRetirement([](const FIntVector&) {});
				Frame.End(FPlatformTime::Seconds());
				co_await FVoxelGameThreadContinuation::Yield();
			}
			Test->TestEqual(TEXT("Original native preparation publishes while external pressure remains held"), Submitted, 1);
			Test->TestEqual(TEXT("Publication does not release or replace the external grant"), Pressure->Serial(), PressureSerial);
			Test->TestEqual(TEXT("Publication does not reduce the external grant"), Pressure->Bytes(), PressureBytes);
			const auto Presented = Renderer.CapturePresentedSnapshot();
			if (!Test->TestTrue(TEXT("Actual scene capture produces a native presented receipt"), Presented.IsValid())) co_return;
			Test->TestEqual(TEXT("The complete frozen transaction is presented"), Presented->GetFineAtomCount(), 8);
			for (const auto& Atom : Atoms)
			{
				const auto* Evidence = Presented->Find(Atom->Section);
				if (!Test->TestNotNull(TEXT("Each actual Fine Atom retains its spatial ownership"), Evidence)) co_return;
				Test->TestTrue(TEXT("Native receipt retains the frozen owner and content"),
					Evidence->Owner == Atom->Evidence.Owner && Evidence->Content == Atom->Evidence.Content && Evidence->bKnownEmpty);
			}
			const auto Final = Resources->Snapshot();
			Test->TestTrue(TEXT("All actual Input allocations stay within the original limit"), Final.Used[0] <= Final.Limits.InputBytes);
			Test->TestEqual(TEXT("Capacity regression keeps the original production Input limit"), Final.Limits.InputBytes, BeforePressure.Limits.InputBytes);
		}
	};
	ADD_LATENT_AUTOMATION_COMMAND(FVoxelTerrainPreparationFrames(
		MakeShared<FVoxelGameThreadContinuation>(FScenario::Execute(this))));
	return true;
}

#endif
