#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationConsumerAdmissionTest,
	"WHFramework.Voxel.FineRefactor.Generation.OriginalConsumerAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationConsumerAdmissionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelTaskScheduler Scheduler;
	const FVoxelGenerationConsumerKey Consumer{EVoxelGenerationConsumerKind::WorldFact, 41, FGuid::NewGuid(), 2,
		EVoxelGenerationSpatialKind::Section, FIntVector(3, 4, 0), FIntVector(3, 4, 0), 93};
	auto MakeRequest = [&Consumer](const uint64 InToken)
	{
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::GenerationPlan;
		Request.WorkClass = EVoxelWorkClass::Critical;
		Request.Stamp.WorldEpoch = Consumer.WorldEpoch;
		Request.Stamp.OwnerId = Consumer.OwnerId;
		Request.Stamp.OwnerIncarnation = Consumer.OwnerIncarnation;
		Request.Stamp.Token = InToken;
		Request.GenerationConsumer = Consumer;
		Request.ReservedBytes = 0;
		Request.Execute = [](const TAtomic<bool>&) { return FVoxelTaskResult(); };
		return Request;
	};
	TestTrue(TEXT("逻辑消费 serial 与发布 token 独立，已授额事实任务无需固定4/8MiB"), Scheduler.Enqueue(MakeRequest(17)));
	FVoxelTaskRequest Foreign = MakeRequest(18);
	Foreign.GenerationConsumer.OwnerId = FGuid::NewGuid();
	TestFalse(TEXT("相同World内错位原 owner 仍拒收"), Scheduler.Enqueue(MoveTemp(Foreign)));
	FVoxelTaskRequest Invalid = MakeRequest(19);
	Invalid.GenerationConsumer.OwnerIncarnation = 0;
	TestFalse(TEXT("不完整消费者拒收"), Scheduler.Enqueue(MoveTemp(Invalid)));
	Scheduler.StopAndJoin();
	return true;
}
#endif
