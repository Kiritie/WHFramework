#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSimulationDemandDeltaTest,
	"WHFramework.Voxel.Streaming.SimulationDeltaSharedRuntimeAndSourceReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSimulationDemandDeltaTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelInterestRuntime Runtime;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -32;
	Manifest.Settings.MaxZ = 32;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource First;
	First.Id = FGuid(1, 2, 3, 4);
	First.Center = FIntVector(8);
	First.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::Collision | EVoxelStreamingCapability::Simulation;
	First.Purpose = EVoxelStreamingSourcePurpose::SimulationAnchor;
	First.ExactRadius = 1;
	First.CollisionRadius = 1;
	First.SimulationRadius = 1;
	First.bInheritWorldView = false;
	First.Normalize();
	FVoxelInterestSet Interest;
	FVoxelInterestDelta Delta;
	bool bSnapshot = false;
	const FIntVector Key = FIntVector::ZeroValue;
	Runtime.Update({First}, Manifest, Settings, Interest, Delta, bSnapshot);
	if (!TestTrue(TEXT("独立模拟Source产生完整需求快照"), bSnapshot && Interest.Exact.Contains(Key))) return false;
	TestTrue(TEXT("新模拟需求产生局部AddedSimulation"), Delta.AddedSimulation.Contains(Key));
	FVoxelStreamingSource Second = First;
	Second.Id = FGuid(5, 6, 7, 8);
	Runtime.Update({First, Second}, Manifest, Settings, Interest, Delta, bSnapshot);
	TestTrue(TEXT("第二Source共享同一模拟区域不重复登记"), bSnapshot && Delta.AddedSimulation.IsEmpty() && Delta.RemovedSimulation.IsEmpty());
	Runtime.Update({Second}, Manifest, Settings, Interest, Delta, bSnapshot);
	TestTrue(TEXT("共享Source未归零时不撤销模拟驻留"), bSnapshot && Delta.RemovedSimulation.IsEmpty() && Interest.Exact.FindChecked(Key).bSimulation);
	Second.Capabilities &= ~EVoxelStreamingCapability::Simulation;
	Runtime.Update({Second}, Manifest, Settings, Interest, Delta, bSnapshot);
	TestTrue(TEXT("模拟引用归零，即使Runtime Exact仍保留也产生RemovedSimulation"),
		bSnapshot && Delta.RemovedSimulation.Contains(Key) && Delta.RemovedExact.IsEmpty() && Interest.Exact.Contains(Key));
	Second.Capabilities |= EVoxelStreamingCapability::Simulation;
	Runtime.Update({Second}, Manifest, Settings, Interest, Delta, bSnapshot);
	TestTrue(TEXT("Source折返复用原Runtime需求，独立恢复模拟事件"),
		bSnapshot && Delta.AddedSimulation.Contains(Key) && Delta.AddedExact.IsEmpty());
	return true;
}

#endif
