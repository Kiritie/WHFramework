#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "Voxel/Task/VoxelTerrainPriority.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainHeterogeneousServiceTest,
	"WHFramework.Voxel.FineResidency.HeterogeneousTwoWorkerService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainHeterogeneousServiceTest::RunTest(const FString& InParameters)
{
	for (const bool bNearCounterexample : {true, false})
	{
		FVoxelTerrainWorkSelection Selection;
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.MaximumWorkers = 2;
		Capacity.NearReservedWorkers = 1;
		TArray<FVoxelTerrainServiceCandidate> Candidates;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FVoxelTerrainServiceCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Index = Index;
			Candidate.Priority.Band = static_cast<EVoxelTerrainServiceBand>(static_cast<uint8>(EVoxelTerrainServiceBand::P0) + Index);
			Candidate.Priority.DistanceCells = Index * 32.0;
			Candidate.EstimatedMicroseconds = Index == (bNearCounterexample ? 0 : 2) ? 100000.0 : 100.0;
			Candidate.Sequence = Index + 1;
			Candidate.bRunnable = true;
		}
		struct FExecution
		{
			int32 Index = INDEX_NONE;
			double EndsAt = 0.0;
			FVoxelTerrainServiceCharge Charge;
		};
		TArray<FExecution> Executions;
		int32 Dispatches[4] = {};
		double Now = 0.0;
		for (int32 Event = 0; Event < 4000; ++Event)
		{
			Capacity.RunningWorkers = Executions.Num();
			Capacity.RunningNearWorkers = 0;
			for (const FExecution& Execution : Executions)
			{
				Capacity.RunningNearWorkers += FVoxelTerrainWorkSelection::IsNear(Execution.Charge.Band) ? 1 : 0;
			}
			while (Executions.Num() < 2)
			{
				FVoxelTerrainServiceCharge Charge;
				const int32 Selected = Selection.Select(Candidates, Capacity, Now, Charge);
				if (!TestTrue(TEXT("A runnable head is selected"), Selected != INDEX_NONE))
				{
					return false;
				}
				++Dispatches[Selected];
				Candidates[Selected].bRunnable = false;
				Executions.Add({Selected, Now + Candidates[Selected].EstimatedMicroseconds / 1000000.0, Charge});
				++Capacity.RunningWorkers;
				Capacity.RunningNearWorkers += FVoxelTerrainWorkSelection::IsNear(Charge.Band) ? 1 : 0;
			}
			int32 Next = 0;
			for (int32 Index = 1; Index < Executions.Num(); ++Index)
			{
				if (Executions[Index].EndsAt < Executions[Next].EndsAt)
				{
					Next = Index;
				}
			}
			const FExecution Completed = Executions[Next];
			Now = Completed.EndsAt;
			TestTrue(TEXT("Actual execution cost settles exactly once"),
				Selection.Settle(Completed.Charge.Serial, Candidates[Completed.Index].EstimatedMicroseconds));
			Candidates[Completed.Index].bRunnable = true;
			Candidates[Completed.Index].FirstQueuedSeconds = Now;
			Candidates[Completed.Index].Sequence += 4;
			Executions.RemoveAtSwap(Next);
		}
		for (const FExecution& Execution : Executions)
		{
			Selection.Settle(Execution.Charge.Serial, Candidates[Execution.Index].EstimatedMicroseconds);
		}
		TestTrue(TEXT("P1 progresses while long P0 and short P2 remain active"), Dispatches[1] > 10);
		TestTrue(TEXT("P3 progresses while long P2 and short near work remain active"), Dispatches[3] > 10);
		TestTrue(TEXT("No accounting fault"), !Selection.HasAccountingFault());
		TestTrue(TEXT("All executions released before reset"), Selection.Reset());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainProxyResolutionFloorTest,
	"WHFramework.Voxel.FineResidency.ProxyResolutionFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainProxyResolutionFloorTest::RunTest(const FString& InParameters)
{
	FString Error;
	FVoxelTerrainGeometry Geometry;
	Geometry.Key = {EVoxelTerrainRepresentation::VoxelProxy, FIntVector(-1), 1};
	Geometry.SampleStepCells = 1;
	TestFalse(TEXT("Step1 cannot enter Proxy geometry"), Geometry.Validate(Error));
	Geometry.SampleStepCells = 2;
	TestTrue(TEXT("Step2 Proxy retains stable spatial identity"), Geometry.Validate(Error));
	int32 Step = 0;
	bool bFloor = false;
	TestTrue(TEXT("Near SSE calculation remains legal"),
		VoxelStreamingGeometry::ResolveSampleStep(0.0, 90.0, 1080, 8.0, 32, Step, bFloor, Error, 2));
	TestEqual(TEXT("Resolution floor preserves half-metre sampling"), Step, 2);
	TestTrue(TEXT("Resolution floor is explicitly reported"), bFloor);
	TestFalse(TEXT("Conflicting maximum cannot silently become step1"),
		VoxelStreamingGeometry::ResolveSampleStep(0.0, 90.0, 1080, 8.0, 1, Step, bFloor, Error, 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainPrioritySnapshotReuseTest,
	"WHFramework.Voxel.FineResidency.PrioritySnapshotReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainPrioritySnapshotReuseTest::RunTest(const FString& InParameters)
{
	FVoxelInterestRuntime Runtime;
	FVoxelViewSettings Settings;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -64;
	Manifest.Settings.MaxZ = 64;
	FVoxelStreamingSource Source;
	Source.Id = FGuid(1, 2, 3, 4);
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::Collision;
	FVoxelInterestSet Initial;
	FVoxelInterestDelta Delta;
	bool bSnapshot = false;
	Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Initial, Delta, bSnapshot);
	if (!TestTrue(TEXT("Initial demand is committed"), bSnapshot && Initial.PlanningError.IsEmpty() && Initial.PublicationInputs.IsValid()))
	{
		return false;
	}
	Source.Priority = 9;
	Source.Direction = FVector::RightVector;
	FVoxelInterestSet Updated;
	const bool bDemandChanged = Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Updated, Delta, bSnapshot);
	TestTrue(TEXT("Priority snapshot is accepted"), bSnapshot);
	TestFalse(TEXT("Rotation and priority do not request a demand publication"), bDemandChanged);
	TestTrue(TEXT("Priority changes freeze new publication service inputs"), Updated.PublicationInputs != Initial.PublicationInputs);
	TestEqual(TEXT("Original exact priority remains unchanged"), Initial.Exact.FindChecked(FIntVector::ZeroValue).ServicePriority.SourcePriority, 0);
	TestEqual(TEXT("Updated exact priority reaches the accepted snapshot"), Updated.Exact.FindChecked(FIntVector::ZeroValue).ServicePriority.SourcePriority, 9);
	TestTrue(TEXT("Fine page plan identity survives priority changes"), Updated.FinePagePlan == Initial.FinePagePlan);
	TestEqual(TEXT("Publication revision is stable"), Updated.PublicationRevision, Initial.PublicationRevision);
	TestEqual(TEXT("Demand revision is stable"), Updated.DemandRevision, Initial.DemandRevision);
	TestTrue(TEXT("Priority revision advances"), Updated.PriorityRevision > Initial.PriorityRevision);
	Settings.Fine.ShardSideCells = 48;
	FVoxelInterestSet Rejected;
	AddExpectedError(TEXT("Invalid terrain policy"), EAutomationExpectedErrorFlags::Contains, 1);
	Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Rejected, Delta, bSnapshot);
	TestFalse(TEXT("Invalid policy does not produce a replacement"), bSnapshot);
	TestTrue(TEXT("Failed planning retains accepted publication inputs"), Rejected.PublicationInputs == Updated.PublicationInputs);
	Settings.Fine.ShardSideCells = 32;
	Source.Priority = 7;
	Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Updated, Delta, bSnapshot);
	TestTrue(TEXT("Accepted source cache survives rejected planning"), bSnapshot &&
		Updated.PublicationRevision == Initial.PublicationRevision && Updated.FinePagePlan == Initial.FinePagePlan);
	TestEqual(TEXT("Retried priority reaches the accepted snapshot"), Updated.Exact.FindChecked(FIntVector::ZeroValue).ServicePriority.SourcePriority, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainSourceTupleTest,
	"WHFramework.Voxel.FineResidency.CompleteSourceTuple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainSourceTupleTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainGeometry Geometry;
	Geometry.Key = {EVoxelTerrainRepresentation::Fine, FIntVector(1, 0, 0), 0};
	TArray<FVoxelStreamingSource> Sources;
	FVoxelStreamingSource& Distant = Sources.AddDefaulted_GetRef();
	Distant.Id = FGuid(1, 0, 0, 0);
	Distant.Priority = -100;
	Distant.Direction = FVector::ForwardVector;
	FVoxelStreamingSource& Near = Sources.AddDefaulted_GetRef();
	Near.Id = FGuid(2, 0, 0, 0);
	Near.Center = FIntVector(8, 0, 0);
	Near.Priority = 100;
	Near.Direction = -FVector::ForwardVector;
	FVoxelTerrainServicePriority Priority;
	FString Error;
	TestTrue(TEXT("Source tuple resolves"), FVoxelTerrainPriority::Resolve(Geometry, {}, Sources, 32, Priority, Error));
	TestEqual(TEXT("Distance is from the winning source"), Priority.DistanceCells, 8.0);
	TestEqual(TEXT("Forward is from the same source"), Priority.ForwardScore, -1.0);
	TestEqual(TEXT("Source priority is from the same source"), Priority.SourcePriority, 100);
	TestEqual(TEXT("Stable identity is from the same source"), Priority.SourceStableKey, FGuid(2, 0, 0, 0));
	Swap(Sources[0], Sources[1]);
	FVoxelTerrainServicePriority Reordered;
	TestTrue(TEXT("Reordered sources resolve"), FVoxelTerrainPriority::Resolve(Geometry, {}, Sources, 32, Reordered, Error));
	TestTrue(TEXT("Input array order cannot alter the winner"), Reordered == Priority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainActualServiceCostTest,
	"WHFramework.Voxel.FineResidency.ActualServiceCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainActualServiceCostTest::RunTest(const FString& InParameters)
{
	const auto RunCost = [this](const double InNearActual)
	{
		FVoxelTerrainWorkSelection Selection;
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.MaximumWaitSeconds = 1000000.0;
		TArray<FVoxelTerrainServiceCandidate> Candidates;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			auto& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Index = Index;
			Candidate.Priority.Band = Index == 0 ? EVoxelTerrainServiceBand::P0 : EVoxelTerrainServiceBand::P3;
			Candidate.Priority.DistanceCells = Index;
			Candidate.EstimatedMicroseconds = 100.0;
			Candidate.Sequence = Index + 1;
			Candidate.bRunnable = true;
		}
		FIntPoint Dispatches = FIntPoint::ZeroValue;
		double Now = 0.0;
		for (int32 Event = 0; Event < 4000; ++Event)
		{
			FVoxelTerrainServiceCharge Charge;
			const int32 Chosen = Selection.Select(Candidates, Capacity, Now, Charge);
			if (!TestTrue(TEXT("实际成本结算后仍可选择可运行任务"), Chosen != INDEX_NONE)) return Dispatches;
			const double Actual = Chosen == 0 ? InNearActual : 100.0;
			TestTrue(TEXT("实际耗时只结算一次"), Selection.Settle(Charge.Serial, Actual));
			TestFalse(TEXT("已结算凭据不能重复扣款"), Selection.Settle(Charge.Serial, Actual));
			Now += Actual / 1000000.0;
			if (Chosen == 0) ++Dispatches.X;
			else ++Dispatches.Y;
		}
		TestFalse(TEXT("成本差额不破坏账本"), Selection.HasAccountingFault());
		TestTrue(TEXT("全部结算后可重置"), Selection.Reset());
		return Dispatches;
	};
	const FIntPoint Cheap = RunCost(100.0);
	const FIntPoint Expensive = RunCost(100000.0);
	TestTrue(TEXT("低估的长任务按真实成本让出后续服务"), Expensive.X < Cheap.X && Expensive.Y > Cheap.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainRefundAndAgingTest,
	"WHFramework.Voxel.FineResidency.RefundAndAging",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainRefundAndAgingTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainWorkSelection Refunded;
	FVoxelTerrainWorkSelection Control;
	FVoxelTerrainServiceCapacity Capacity;
	TArray<FVoxelTerrainServiceCandidate> Candidates;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Index = Index;
		Candidate.Priority.Band = Index == 0 ? EVoxelTerrainServiceBand::P0 : EVoxelTerrainServiceBand::P3;
		Candidate.Priority.DistanceCells = Index;
		Candidate.EstimatedMicroseconds = 100.0;
		Candidate.Sequence = Index + 1;
		Candidate.bRunnable = true;
	}
	for (int32 Attempt = 0; Attempt < 1000; ++Attempt)
	{
		FVoxelTerrainServiceCharge Charge;
		if (!TestEqual(TEXT("未开工退款保留出生任务服务额度"), Refunded.Select(Candidates, Capacity, 0.0, Charge), 0)) return false;
		TestFalse(TEXT("未结清凭据阻止重置"), Refunded.Reset());
		TestTrue(TEXT("未开工凭据可退款"), Refunded.RefundBeforeStart(Charge.Serial));
		TestFalse(TEXT("同一凭据不能二次退款"), Refunded.RefundBeforeStart(Charge.Serial));
		TestFalse(TEXT("退款后不能再次结算"), Refunded.Settle(Charge.Serial, 100.0));
	}
	for (int32 Attempt = 0; Attempt < 256; ++Attempt)
	{
		FVoxelTerrainServiceCharge RefundedCharge;
		FVoxelTerrainServiceCharge ControlCharge;
		const int32 RefundedChoice = Refunded.Select(Candidates, Capacity, 0.0, RefundedCharge);
		const int32 ControlChoice = Control.Select(Candidates, Capacity, 0.0, ControlCharge);
		if (!TestTrue(TEXT("两份账本均有可运行任务"), RefundedChoice != INDEX_NONE && ControlChoice != INDEX_NONE)) return false;
		TestEqual(TEXT("退款不改变后续已执行任务的成本份额"), RefundedChoice, ControlChoice);
		TestTrue(TEXT("退款路径后续任务可结算"), Refunded.Settle(RefundedCharge.Serial, 100.0));
		TestTrue(TEXT("对照任务可结算"), Control.Settle(ControlCharge.Serial, 100.0));
	}
	TestTrue(TEXT("退款账本可重置"), Refunded.Reset());
	Candidates[0].Priority.Band = Candidates[1].Priority.Band = EVoxelTerrainServiceBand::P2;
	Candidates[0].FirstQueuedSeconds = 1.0;
	Candidates[1].FirstQueuedSeconds = 0.0;
	FVoxelTerrainServiceCharge Charge;
	TestEqual(TEXT("同一优先带的较远超期任务能够推进"), Refunded.Select(Candidates, Capacity, 1.0, Charge), 1);
	TestTrue(TEXT("超期任务按实际成本结算"), Refunded.Settle(Charge.Serial, 100.0));
	FVoxelTerrainServicePriority Birth;
	Birth.Band = EVoxelTerrainServiceBand::P0;
	Birth.DistanceCells = 1024.0;
	FVoxelTerrainServicePriority Background;
	Background.Band = EVoxelTerrainServiceBand::P3;
	Background.DistanceCells = 1.0;
	TestTrue(TEXT("出生关键任务优先于距离更近的背景任务"), FVoxelTerrainWorkSelection::HigherPriority(Birth, Background));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainPublicationDrainTest,
	"WHFramework.Voxel.FineResidency.PublicationDrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainPublicationDrainTest::RunTest(const FString& InParameters)
{
	FVoxelTerrainServiceCandidate Generation;
	Generation.Index = 0;
	Generation.Priority.Band = EVoxelTerrainServiceBand::P2;
	Generation.Priority.DistanceCells = 1.0;
	Generation.FirstQueuedSeconds = 0.0;
	Generation.Sequence = 1;
	Generation.EstimatedMicroseconds = 100.0;
	Generation.bRunnable = true;
	FVoxelTerrainServiceCandidate Publication = Generation;
	Publication.Index = 1;
	Publication.Priority.DistanceCells = 1000.0;
	Publication.FirstQueuedSeconds = 1.0;
	Publication.Sequence = 2;
	Publication.bPublicationContinuation = true;
	TestTrue(TEXT("非近景发布续算优先于同带的新正文"),
		FVoxelTerrainWorkSelection::HigherPriority(Publication, Generation));
	FVoxelTerrainWorkSelection Selection;
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.bPreserveDistanceOrder = true;
	Capacity.MaximumWaitSeconds = 1000000.0;
	for (int32 Turn = 0; Turn < 32; ++Turn)
	{
		FVoxelTerrainServiceCandidateSet Candidates;
		Candidates.Add(Generation);
		Candidates.Add(Publication);
		FVoxelTerrainServiceCharge Charge;
		const int32 Chosen = Selection.Select(Candidates.GetCandidates(), Capacity, 2.0, Charge);
		TestEqual(TEXT("发布排空连续推进，同时每四次保留最老正文机会"), Chosen, Turn % 4 == 3 ? 0 : 1);
		TestTrue(TEXT("发布优先仍按实际耗时结算"), Selection.Settle(Charge.Serial, 100.0));
	}
	TestFalse(TEXT("轮转不破坏服务账本"), Selection.HasAccountingFault());
	for (const auto Band : {EVoxelTerrainServiceBand::P0, EVoxelTerrainServiceBand::P1})
	{
		Generation.Priority.Band = Publication.Priority.Band = Band;
		TestTrue(TEXT("关键近景始终保持原距离顺序"),
			FVoxelTerrainWorkSelection::HigherPriority(Generation, Publication));
	}
	return true;
}

#endif
