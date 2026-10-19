#include "Gameplay/WHGameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveGamePendingLoadTransferTest, "WHFramework.SaveGame.LoadRequest.TransferIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveGamePendingLoadTransferTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWHGameInstance> Instance(NewObject<UWHGameInstance>());
	FSaveGameLoadSnapshot First;
	First.Context.OperationId = FGuid::NewGuid();
	First.Context.SaveId = FGuid::NewGuid();
	First.Context.Generation = 7;
	First.Context.TargetMap = TEXT("/Game/Maps/LoadTargetA");
	First.Context.LoadPhase = EPhase::PrimaryAndLesser;
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/LoadSnapshot"), First.Context.OperationId.ToString());
	First.ReadLease = FSaveGameGenerationReadLease::Acquire(Directory);
	TestTrue(TEXT("完整快照可以应用首阶段"), First.CanApply(EPhase::PrimaryAndLesser));
	TestFalse(TEXT("首阶段之前不能应用Final"), First.CanApply(EPhase::Final));
	FSaveGameLoadSnapshot Invalid = First;
	Invalid.Context.OperationId.Invalidate();
	TestFalse(TEXT("无操作身份的切图请求不能接纳"), Instance->SetPendingSaveLoad(Invalid));
	Invalid = First;
	Invalid.ReadLease.Reset();
	TestFalse(TEXT("无读租约的切图请求不能接纳"), Instance->SetPendingSaveLoad(Invalid));
	Invalid = FSaveGameLoadSnapshot();
	TestTrue(TEXT("接纳完整的首个切图快照"), Instance->SetPendingSaveLoad(First));
	FSaveGameLoadSnapshot Second = First;
	Second.Context.OperationId = FGuid::NewGuid();
	Second.Context.SaveId = FGuid::NewGuid();
	Second.Context.Generation = 99;
	Second.Context.TargetMap = TEXT("/Game/Maps/LoadTargetB");
	TestFalse(TEXT("未消费请求不能被后来的不同快照覆盖"), Instance->SetPendingSaveLoad(Second));
	Instance->CancelPendingSaveLoad(Second.Context.OperationId);
	FSaveGameLoadSnapshot Taken = Instance->ConsumePendingSaveLoad();
	TestEqual(TEXT("错误身份的取消不影响首个请求"), Taken.Context.OperationId, First.Context.OperationId);
	TestEqual(TEXT("移交保留原存档身份"), Taken.Context.SaveId, First.Context.SaveId);
	TestEqual(TEXT("移交保留冻结文件代"), Taken.Context.Generation, First.Context.Generation);
	TestEqual(TEXT("移交保留目标地图"), Taken.Context.TargetMap, First.Context.TargetMap);
	TestTrue(TEXT("移交保留同一读租约"), Taken.ReadLease == First.ReadLease);
	TestTrue(TEXT("移交保留请求加载阶段"), Taken.Context.LoadPhase == First.Context.LoadPhase);
	Taken.AppliedPhases = EPhase::PrimaryAndLesser;
	TestTrue(TEXT("首阶段成功后允许Final"), Taken.CanApply(EPhase::Final));
	TestFalse(TEXT("禁止重复应用已完成阶段"), Taken.CanApply(EPhase::Primary));
	TestFalse(TEXT("同一快照只移交一次"), Instance->ConsumePendingSaveLoad().IsValid());
	TestTrue(TEXT("移交后可以接纳下一请求"), Instance->SetPendingSaveLoad(Second));
	Instance->CancelPendingSaveLoad(First.Context.OperationId);
	TestEqual(TEXT("旧请求取消不能清理新请求"), Instance->ConsumePendingSaveLoad().Context.OperationId, Second.Context.OperationId);
	TestTrue(TEXT("再次接纳供匹配取消验证"), Instance->SetPendingSaveLoad(First));
	const FGuid OperationId = First.Context.OperationId;
	First = FSaveGameLoadSnapshot();
	Second = FSaveGameLoadSnapshot();
	Taken = FSaveGameLoadSnapshot();
	TestEqual(TEXT("只剩GameInstance持有租约时仍固定文件代"), FSaveGameGenerationReadLease::GetPinCount(Directory), uint64(1));
	Instance->CancelPendingSaveLoad(OperationId);
	TestFalse(TEXT("匹配取消后不再有待移交快照"), Instance->ConsumePendingSaveLoad().IsValid());
	TestEqual(TEXT("匹配取消释放最后的文件代租约"), FSaveGameGenerationReadLease::GetPinCount(Directory), uint64(0));
	return true;
}

#endif
