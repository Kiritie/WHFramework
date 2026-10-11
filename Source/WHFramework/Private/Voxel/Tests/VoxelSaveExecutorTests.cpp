#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SaveGame/SaveGameAsyncExecutor.h"
#include "SaveGame/SaveGameStorage.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelSceneSaveCapture.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSaveExecutorCapacityCutTest,
	"WHFramework.Voxel.Save.ExecutorCapacityCut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSaveExecutorCapacityCutTest::RunTest(const FString& InParameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelSaveExecutor"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FSaveGameStorage Storage(0, Root);
	FSaveManifest Manifest;
	Manifest.SaveId = FGuid::NewGuid();
	if (!TestTrue(TEXT("Capacity save creates its actual slot and manifest"), Storage.EnsureRoot() &&
		Storage.CreateWorldDirectory(Manifest.SaveId) && Storage.WriteManifestAtomic(Manifest.SaveId, Manifest))) return false;
	const auto Registry = VoxelTest::MakeRegistry();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	FVoxelWorldRuntime Runtime(81, true, Registry, VoxelTest::MakeGenerator());
	Runtime.BindTerrainResources(Resources);
	FVoxelWorldManifest WorldManifest;
	WorldManifest.WorldId = FGuid::NewGuid();
	WorldManifest.RegistryHash = Registry->Hash;
	WorldManifest.RecipeHash = VoxelTest::MakeGenerationConfig()->Recipe->RecipeHash;
	const FIntVector Key(-3, 4, 1);
	FString Error;
	const TCHAR* Stage = TEXT("runtime setup");
	bool bCompleted = false;
	ON_SCOPE_EXIT
	{
		if (!bCompleted) AddError(FString::Printf(TEXT("Capacity save stopped at %s: %s"), Stage, *Error));
	};
	const auto* Section = Runtime.FindOrAllocate(Key, 0);
	if (!TestNotNull(TEXT("Capacity save allocates its original runtime section"), Section)) return false;
	TArray<FVoxelBlockState> Base;
	Base.Init(FVoxelBlockState(), VoxelBlock::Volume);
	if (!TestTrue(TEXT("Capacity save publishes its actual section"), Runtime.PublishBase(Key, Section->Stamp, MoveTemp(Base), Error) &&
		Runtime.PublishFinal(Key, 0, {}, {}, Error))) { AddError(Error); return false; }
	FVoxelPreparedEdit Prepared;
	FVoxelEditBatch Batch;
	if (!TestTrue(TEXT("Capacity save commits the original frozen content"),
		Runtime.PrepareEdit({{Key * 16, FVoxelBlockState(), FVoxelBlockState(1, 0)}}, {}, Prepared, Error) &&
		Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error))) { AddError(Error); return false; }
	FVoxelRegionStore Store;
	FVoxelWorldSaveAdapter Adapter;
	auto MakePlan = [&](bool bInCreatedSlot, FSaveGenerationPlan& OutPlan)
	{
		if (!TestTrue(TEXT("Capacity save captures its original journal and resources"),
			Adapter.Capture(Runtime, WorldManifest, Registry, Store, Error))) { AddError(Error); return false; }
		const auto& Captured = *Adapter.GetCapture();
		auto Scenes = MakeShared<FVoxelSceneSaveCapture, ESPMode::ThreadSafe>();
		Scenes->TransactionId = Captured.TransactionId;
		Scenes->WorldEpoch = Captured.WorldEpoch;
		Scenes->Completion = Captured.Completion;
		if (!TestTrue(TEXT("Capacity save attaches the same transaction scene cut"), Adapter.AttachSceneCapture(Scenes))) return false;
		Captured.Completion->ExpectedSaveId = Manifest.SaveId;
		Captured.Completion->ExpectedStorageRoot = MakeShared<const FString, ESPMode::ThreadSafe>(Storage.GetRootDir());
		OutPlan = FSaveGenerationPlan();
		OutPlan.TransactionId = Captured.TransactionId;
		OutPlan.UserIndex = 0;
		OutPlan.StorageRoot = Root;
		OutPlan.SaveId = Manifest.SaveId;
		OutPlan.Manifest = Manifest;
		OutPlan.Voxel.Emplace();
		OutPlan.Voxel->Voxels = *Adapter.GetCapture();
		OutPlan.Voxel->Scenes = Scenes;
		OutPlan.bResolveManifestOnWorker = true;
		OutPlan.bDeleteCreatedSlotOnFailure = bInCreatedSlot;
		return true;
	};
	auto WaitForCapacity = [&](FSaveGameAsyncExecutor& InExecutor)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (!InExecutor.IsWaitingForCapacity() && FPlatformTime::Seconds() < Deadline)
			FPlatformProcess::SleepNoStats(0.001f);
		return InExecutor.IsWaitingForCapacity();
	};
	FVoxelTerrainFrameContext Frame;
	uint64 FrameCounter = 0;
	if (!TestTrue(TEXT("Capacity save configures its original publication frame"),
		Frame.Configure(FVoxelTerrainFrameBudgetSettings(), Error))) { AddError(Error); return false; }
	auto WaitForResult = [&](FSaveGameAsyncExecutor& InExecutor, FSaveOperationResult& OutResult)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (FPlatformTime::Seconds() < Deadline)
		{
			if (!Frame.Begin(81, ++FrameCounter, FPlatformTime::Seconds(), {})) return false;
			const bool bCompleted = InExecutor.Poll(OutResult, &Frame);
			Frame.End(FPlatformTime::Seconds());
			if (bCompleted) return true;
			FPlatformProcess::SleepNoStats(0.001f);
		}
		return false;
	};
	FSaveGenerationPlan Plan;
	Stage = TEXT("first frozen capture");
	if (!MakePlan(true, Plan)) return false;
	const auto Completion = Plan.Voxel->Voxels.Completion;
	const auto BlockerOwner = FVoxelGenerationConsumerKey::Section(81, FGuid::NewGuid(), 1, Key, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto HoldWorkingCapacity = [&]()
	{
		if (!TestTrue(TEXT("Capacity blocker admits its original lease"),
			Resources->Acquire(BlockerOwner, EVoxelTerrainResourcePool::Working, 1, Blocker))) return false;
		const auto Capacity = Resources->Snapshot();
		return TestTrue(TEXT("Capacity blocker withholds all remaining original Working bytes"),
			Blocker->Resize(1 + Capacity.Limits.WorkingBytes - Capacity.Used[static_cast<int32>(EVoxelTerrainResourcePool::Working)]));
	};
	if (!HoldWorkingCapacity()) return false;
	FSaveGameAsyncExecutor Executor;
	if (!TestTrue(TEXT("完整冻结切点被原生任务接纳"), Executor.Start(MoveTemp(Plan), &Error)) ||
		!TestTrue(TEXT("实际任务退出后保留容量等待"), WaitForCapacity(Executor))) return false;
	TestEqual(TEXT("目标代在第一次实际读取 manifest 后冻结"), Completion->TargetGeneration, 1);
	FSaveManifest Uncommitted;
	TestTrue(TEXT("容量等待不得删除已接纳的新槽"), Storage.ReadManifest(Manifest.SaveId, Uncommitted));
	TestEqual(TEXT("容量等待不得发布 manifest"), Uncommitted.CurrentGeneration, 0);
	FSaveOperationResult Result;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		TestFalse(TEXT("没有外部释放时不重新启动或报告完成"), Executor.Poll(Result));
		TestTrue(TEXT("等待期间原冻结事务仍由 Executor 拥有"), Executor.IsWaitingForCapacity());
	}
	Blocker.Reset();
	Stage = TEXT("frame-controlled capacity retry");
	TestFalse(TEXT("有外部释放仍不能从无 Frame 的普通 Tick 派发重试"), Executor.Poll(Result));
	TestTrue(TEXT("无 Frame 继续持有同一冻结事务"), Executor.IsWaitingForCapacity());
	if (!Frame.Begin(82, ++FrameCounter, FPlatformTime::Seconds(), {})) return false;
	TestFalse(TEXT("另一个世界的 Frame 不得派发原事务"), Executor.Poll(Result, &Frame));
	TestTrue(TEXT("跨世界拒绝仍保持原容量等待"), Executor.IsWaitingForCapacity());
	Frame.End(FPlatformTime::Seconds());
	if (!Frame.Begin(81, ++FrameCounter, FPlatformTime::Seconds(), {})) return false;
	const double BudgetStarted = FPlatformTime::Seconds();
	Frame.GetPublishBudget().Begin(BudgetStarted, 1000.0, 1);
	if (!Frame.GetPublishBudget().TryStart(EVoxelPublishPhase::Plan, BudgetStarted, 0.0)) return false;
	Frame.GetPublishBudget().Finish(EVoxelPublishPhase::Plan, BudgetStarted, FPlatformTime::Seconds());
	Frame.ConsumeOverdueOpportunity();
	TestFalse(TEXT("原 Frame 已耗尽预算与 overdue 时不派发"), Executor.Poll(Result, &Frame));
	TestTrue(TEXT("预算拒绝保留原容量等待"), Executor.IsWaitingForCapacity());
	TestEqual(TEXT("没有计入实际 Prepare 操作"), Frame.GetPublishBudget().GetTiming(EVoxelPublishPhase::Prepare).Operations, 0);
	Frame.End(FPlatformTime::Seconds());
	Stage = TEXT("later edit while original save cut waits");
	if (!Runtime.PrepareEdit({{Key * 16, FVoxelBlockState(1, 0), FVoxelBlockState()}}, {}, Prepared, Error) ||
		!Runtime.CommitPreparedEdit(MoveTemp(Prepared), Batch, Error)) return false;
	if (!TestTrue(TEXT("外部释放后原实际任务重试并提交"), WaitForResult(Executor, Result)) ||
		!TestTrue(TEXT("原事务原生提交成功"), bool(Result))) return false;
	TestTrue(TEXT("原目标代回执完成"), Completion->bFilesFlushed && Completion->bGenerationCommitted);
	TestTrue(TEXT("原后台事务已经准备并授予目标文件代读租约"), Completion->TargetReadLease &&
		Completion->TargetReadLease->Matches(Storage.GetGenerationDir(Manifest.SaveId, 1)) &&
		Completion->TargetReadLease->IsResourceAccountedFor(&Resources.Get(), 81));
	FVoxelRegionStore ReadStore;
	ReadStore.AdvanceSource(Storage.GetGenerationDir(Manifest.SaveId, 1));
	TArray<uint8> ReadBytes;
	FVoxelPersistentSection Saved;
	if (!TestTrue(TEXT("重试后真实文件可读回"), ReadStore.ReadSection(Key, ReadBytes, Error) == EVoxelRegionRead::Loaded &&
		FVoxelDeltaCodec::Decode(ReadBytes, WorldManifest, *Registry, Saved))) return false;
	TestEqual(TEXT("重试保留原切点，没有重捕获后续编辑"), Saved.Revision, uint64(1));
	TestTrue(TEXT("原冻结区块内容保持"), Saved.Blocks.FindChecked(0) == FVoxelBlockState(1, 0));
	TestTrue(TEXT("原回执按实际文件代结算"), Adapter.Complete(Runtime, Store, true, Storage.GetGenerationDir(Manifest.SaveId, 1)));
	TestTrue(TEXT("GT目录回执复用原Worker租约"), Store.GetSourceLease() == Completion->TargetReadLease);
	TestEqual(TEXT("后续编辑仍保留脏内容版本"), Runtime.FindCommittedOverlay(Key)->Revision, uint64(2));
	const double RetireDeadline = FPlatformTime::Seconds() + 5.0;
	Stage = TEXT("first capture retirement");
	while (!Adapter.CanCapture() && FPlatformTime::Seconds() < RetireDeadline) FPlatformProcess::SleepNoStats(0.001f);
	if (!Storage.ReadManifest(Manifest.SaveId, Manifest) || !Adapter.CanCapture()) return false;
	if (!MakePlan(false, Plan) || !HoldWorkingCapacity()) return false;
	const auto ConflictCompletion = Plan.Voxel->Voxels.Completion;
	Stage = TEXT("conflicting generation capacity wait");
	if (!Executor.Start(MoveTemp(Plan), &Error) || !WaitForCapacity(Executor)) return false;
	TestEqual(TEXT("第二原切点的目标代已冻结"), ConflictCompletion->TargetGeneration, 2);
	Stage = TEXT("external generation prepare");
	if (!TestTrue(TEXT("外部写入者准备原目标代临时目录"), Storage.PrepareTempGeneration(Manifest.SaveId, 1, 2))) return false;
	Stage = TEXT("external generation flush");
	if (!TestTrue(TEXT("外部写入者持久化原目标代全部文件"), Storage.FlushTempGeneration(Manifest.SaveId, 2))) return false;
	Stage = TEXT("external generation commit");
	if (!TestTrue(TEXT("外部写入者提交原目标代目录"), Storage.CommitGeneration(Manifest.SaveId, 2))) return false;
	Manifest.CurrentGeneration = 2;
	if (!Storage.WriteManifestAtomic(Manifest.SaveId, Manifest)) return false;
	Blocker.Reset();
	Stage = TEXT("same cut detects externally committed generation");
	if (!WaitForResult(Executor, Result)) return false;
	TestEqual(TEXT("等待期间代冲突失败，不能给旧切点改选新目标"), Result.Code, ESaveResultCode::CommitFailed);
	TestEqual(TEXT("失败保留原目标代身份"), ConflictCompletion->TargetGeneration, 2);
	Adapter.Complete(Runtime, Store, false, FString());
	const double CancelRetireDeadline = FPlatformTime::Seconds() + 5.0;
	while (!Adapter.CanCapture() && FPlatformTime::Seconds() < CancelRetireDeadline) FPlatformProcess::SleepNoStats(0.001f);
	Manifest.SaveId = FGuid::NewGuid();
	Manifest.CurrentGeneration = 0;
	Manifest.LastCommitOperationId.Invalidate();
	Stage = TEXT("cancelable created slot capture");
	if (!TestTrue(TEXT("冲突切点退休后可以捕获新槽"), Adapter.CanCapture()) ||
		!TestTrue(TEXT("取消夹具创建独立新槽"), Storage.CreateWorldDirectory(Manifest.SaveId)) ||
		!TestTrue(TEXT("独立新槽写入无旧提交身份的gen0清单"), Storage.WriteManifestAtomic(Manifest.SaveId, Manifest)) ||
		!MakePlan(true, Plan) || !HoldWorkingCapacity()) return false;
	if (!Executor.Start(MoveTemp(Plan), &Error) || !WaitForCapacity(Executor)) return false;
	Executor.RequestCancel();
	if (!TestTrue(TEXT("取消容量等待无需先释放容量，也不在 GT 执行原生清理"), WaitForResult(Executor, Result))) return false;
	TestEqual(TEXT("取消返回原事务终止结果"), Result.Code, ESaveResultCode::Busy);
	TestFalse(TEXT("实际取消 Worker 清理自己仍未提交的新槽"), IFileManager::Get().DirectoryExists(*Storage.GetWorldDir(Manifest.SaveId)));
	Blocker.Reset();
	Adapter.Complete(Runtime, Store, false, FString());
	ReadStore.Reset();
	Store.Reset();
	bCompleted = true;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSaveExecutorSlotOwnershipTest,
	"WHFramework.Voxel.Save.ExecutorSlotOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSaveExecutorSlotOwnershipTest::RunTest(const FString& InParameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelSaveOwnership"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FGuid TransactionId = FGuid::NewGuid();
	FSaveGameStorage Writer(0, Root, TransactionId);
	FSaveGameStorage Other(0, Root);
	FSaveGameStorage SameTransaction(0, Root, TransactionId);
	FSaveGameAsyncExecutor Executor;
	ON_SCOPE_EXIT
	{
		Writer.EndWorldWrite();
		Other.EndWorldWrite();
		if (!Executor.IsRunning()) FSaveGameGenerationReadLease::TryRetireDirectory(Root);
	};
	FSaveManifest Manifest;
	Manifest.SaveId = FGuid::NewGuid();
	if (!Writer.EnsureRoot() || !Writer.CreateWorldDirectory(Manifest.SaveId) ||
		!Writer.WriteManifestAtomic(Manifest.SaveId, Manifest)) return false;
	TestEqual(TEXT("同一冻结事务使用同一临时目录"), Writer.GetTempGenerationDir(Manifest.SaveId, 1),
		SameTransaction.GetTempGenerationDir(Manifest.SaveId, 1));
	TestNotEqual(TEXT("不同事务的临时目录独立"), Writer.GetTempGenerationDir(Manifest.SaveId, 1),
		Other.GetTempGenerationDir(Manifest.SaveId, 1));
	if (!TestEqual(TEXT("原事务取得槽位所有权"), Writer.BeginWorldWrite(Manifest.SaveId, nullptr, {}),
		ESaveWorldWriteResult::Acquired)) return false;
	TestEqual(TEXT("相同事务 GUID 不能代替实际活跃租约"), SameTransaction.BeginWorldWrite(Manifest.SaveId, nullptr, {}),
		ESaveWorldWriteResult::Busy);
	TestEqual(TEXT("其它事务等待原生槽位所有者"), Other.BeginWorldWrite(Manifest.SaveId, nullptr, {}), ESaveWorldWriteResult::Busy);
	if (!Writer.PrepareTempGeneration(Manifest.SaveId, 0, 1)) return false;
	const FString Temp = Writer.GetTempGenerationDir(Manifest.SaveId, 1);
	TestFalse(TEXT("其它 writer 不能清理原临时目录"), Other.CleanupTempGenerations(Manifest.SaveId));
	TestFalse(TEXT("其它 writer 不能删除整个槽"), Other.DeleteWorldDirectory(Manifest.SaveId));
	TestFalse(TEXT("其它 writer 不能改 manifest"), Other.WriteManifestAtomic(Manifest.SaveId, Manifest));
	Other.CleanupAllTempGenerations();
	TestTrue(TEXT("启动清理保留活跃 writer 的目录"), IFileManager::Get().DirectoryExists(*Temp));
	TestFalse(TEXT("裸目录退休不能越过原 writer"), FSaveGameGenerationReadLease::TryRetireDirectory(Writer.GetWorldDir(Manifest.SaveId)));
	FSaveManifest Independent;
	Independent.SaveId = FGuid::NewGuid();
	TestTrue(TEXT("独立槽可继续创建和发布"), Other.CreateWorldDirectory(Independent.SaveId) &&
		Other.WriteManifestAtomic(Independent.SaveId, Independent));
	auto MakePlan = [&]()
	{
		FSaveGenerationPlan Plan;
		Plan.StorageRoot = Root;
		Plan.SaveId = Manifest.SaveId;
		Plan.Manifest = Manifest;
		Plan.TransactionId = FGuid::NewGuid();
		FSaveModuleBytes Module;
		Module.ModuleName = FName(TEXT("OwnershipModule"));
		Module.Version = 1;
		Module.Bytes = {3, 7, 19};
		Plan.Modules.Add(MoveTemp(Module));
		return Plan;
	};
	auto WaitForSlot = [&]()
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (!Executor.IsWaitingForSlot() && FPlatformTime::Seconds() < Deadline) FPlatformProcess::SleepNoStats(0.001f);
		return Executor.IsWaitingForSlot();
	};
	auto WaitForResult = [&](FSaveOperationResult& OutResult)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (FPlatformTime::Seconds() < Deadline)
		{
			if (Executor.Poll(OutResult)) return true;
			FPlatformProcess::SleepNoStats(0.001f);
		}
		return false;
	};
	FString Error;
	if (!Executor.Start(MakePlan(), &Error) || !WaitForSlot()) return false;
	FSaveOperationResult Result;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		TestFalse(TEXT("没有释放事件时不重新派发等待事务"), Executor.Poll(Result));
		TestTrue(TEXT("实际冻结事务仍在槽位等待"), Executor.IsWaitingForSlot());
	}
	Executor.RequestCancel();
	if (!TestTrue(TEXT("取消无需等待或删除其它 writer"), WaitForResult(Result))) return false;
	TestEqual(TEXT("取消返回原事务终止结果"), Result.Code, ESaveResultCode::Busy);
	TestTrue(TEXT("取消保留原 writer 的临时目录"), IFileManager::Get().DirectoryExists(*Temp));
	if (!Executor.Start(MakePlan(), &Error) || !WaitForSlot()) return false;
	const uint64 ReleaseBefore = FSaveGameGenerationWriteLease::ReleaseVersion();
	Writer.EndWorldWrite();
	TestTrue(TEXT("实际 writer 退出推进释放版本"), FSaveGameGenerationWriteLease::ReleaseVersion() > ReleaseBefore);
	if (!TestTrue(TEXT("释放后继续同一原生事务并提交"), WaitForResult(Result)) || !TestTrue(TEXT("真实写入完成"), bool(Result))) return false;
	FSaveManifest Committed;
	TArray<uint8> Bytes;
	TestTrue(TEXT("实际 manifest 和模块正文对应同一文件代"), Other.ReadManifest(Manifest.SaveId, Committed) &&
		Committed.CurrentGeneration == 1 && Other.ReadBinary(Other.GetModuleFilePath(Manifest.SaveId, 1,
		FName(TEXT("OwnershipModule"))), Bytes) && Bytes == TArray<uint8>({3, 7, 19}));
	const auto ReadLease = FSaveGameGenerationReadLease::Acquire(Other.GetGenerationDir(Manifest.SaveId, 1));
	if (!ReadLease || Other.BeginWorldWrite(Manifest.SaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired) return false;
	TestFalse(TEXT("自己的 writer 仍不能越过已提交读租约删除目录"), Other.DeleteWorldDirectory(Manifest.SaveId));
	TestTrue(TEXT("已有读租约允许下一代准备"), Other.PrepareTempGeneration(Manifest.SaveId, 1, 2));
	TestTrue(TEXT("读取期间只清理原 writer 的未提交内容"), Other.CleanupTempGenerations(Manifest.SaveId));
	TestTrue(TEXT("读者的不可变文件代仍存在"), IFileManager::Get().DirectoryExists(*ReadLease->Directory()));
	Other.EndWorldWrite();
	return true;
}

#endif
