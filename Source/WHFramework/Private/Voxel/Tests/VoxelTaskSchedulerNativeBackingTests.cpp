#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

struct FVoxelSchedulerNativeBackingTestAccess
{
	static UE::Tasks::FTask HoldNative(const FVoxelTaskScheduler& InScheduler)
	{
		if (InScheduler.Running.IsEmpty()) return UE::Tasks::FTask();
		return static_cast<const UE::Tasks::FTask&>(InScheduler.Running[0].Task);
	}

	static const void* ExecutionReservation(const FVoxelTaskScheduler& InScheduler)
	{
		if (!InScheduler.Pending.IsEmpty())
		{
			return InScheduler.Pending[0].ExecutionReservation.Get();
		}
		if (!InScheduler.Running.IsEmpty())
		{
			return InScheduler.Running[0].ExecutionReservation.Get();
		}
		return nullptr;
	}

	static FGuid Receipt(const FVoxelTaskScheduler& InScheduler)
	{
		if (!InScheduler.Pending.IsEmpty() && InScheduler.Pending[0].WorkReceipt)
			return InScheduler.Pending[0].WorkReceipt->GetAttemptId();
		if (!InScheduler.Running.IsEmpty() && InScheduler.Running[0].Slot->WorkReceipt)
			return InScheduler.Running[0].Slot->WorkReceipt->GetAttemptId();
		return FGuid();
	}

	static uint64 RootInputBytes(const FVoxelTaskScheduler& InScheduler)
	{
		return InScheduler.RootInputBytes;
	}
};

namespace
{
	struct FNativeBackingState
	{
		explicit FNativeBackingState(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources)
			: Resources(InResources)
		{
			Release = FPlatformProcess::GetSynchEventFromPool(true);
		}

		~FNativeBackingState()
		{
			Release->Trigger();
			FPlatformProcess::ReturnSynchEventToPool(Release);
		}

		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FEvent* Release = nullptr;
		TAtomic<int32> Copies{0};
		TAtomic<int32> Executions{0};
		TAtomic<bool> AllowContinuation{false};
		TAtomic<bool> Entered{false};
		TAtomic<bool> CancelObserved{false};
		TAtomic<bool> GrantObserved{true};
		TAtomic<bool> ConsumerObserved{true};
		TAtomic<bool> PressureExpected{false};
		TAtomic<bool> PressureObserved{true};
	};

	struct FCountingExecute
	{
		FCountingExecute(TSharedRef<FNativeBackingState, ESPMode::ThreadSafe> InState,
			const FVoxelGenerationConsumerKey& InConsumer, const bool bInBlock)
			: State(InState), Consumer(InConsumer), bBlock(bInBlock)
		{
		}

		FCountingExecute(const FCountingExecute& InOther)
			: State(InOther.State), Consumer(InOther.Consumer), bBlock(InOther.bBlock)
		{
			++State->Copies;
		}

		FCountingExecute(FCountingExecute&&) = default;
		~FCountingExecute() = default;

		FVoxelTaskResult operator()(const TAtomic<bool>& InCancel) const
		{
			const auto Snapshot = State->Resources->Snapshot();
			if (!Snapshot.Used[int32(EVoxelTerrainResourcePool::Input)] || !Snapshot.Used[int32(EVoxelTerrainResourcePool::Working)])
				State->GrantObserved.Store(false);
			const auto* Active = FVoxelGenerationConsumerScope::GetConsumerKey();
			if (!Active || *Active != Consumer) State->ConsumerObserved.Store(false);
			const int32 Execution = ++State->Executions;
			if (Execution > 1 && State->PressureExpected.Load() &&
				(Snapshot.Used[int32(EVoxelTerrainResourcePool::Input)] != Snapshot.Limits.InputBytes ||
					Snapshot.Used[int32(EVoxelTerrainResourcePool::Working)] != Snapshot.Limits.WorkingBytes))
			{
				State->PressureObserved.Store(false);
			}
			if (bBlock)
			{
				State->Entered.Store(true);
				State->Release->Wait();
				State->CancelObserved.Store(InCancel.Load());
			}
			FVoxelTaskResult Result;
			Result.bSuccess = true;
			Result.bContinue = !bBlock && Execution < 4;
			return Result;
		}

		TSharedRef<FNativeBackingState, ESPMode::ThreadSafe> State;
		FVoxelGenerationConsumerKey Consumer;
		bool bBlock = false;
		uint8 Payload[512] = {};
	};

	enum class EBackingPhase : uint8
	{
		None = 0,
		HeldNative,
		Continuation,
		CapacityWait,
		Finished,
		CancelEntered,
		Canceled,
		Drain
	};

	class FNativeBackingCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FNativeBackingCommand(FAutomationTestBase& InTest)
			: Test(InTest)
		{
			Deadline = FPlatformTime::Seconds() + 15.0;
		}

		~FNativeBackingCommand()
		{
			if (State) State->Release->Trigger();
		}

		virtual bool Update() override
		{
			if (Phase == EBackingPhase::None)
			{
				const auto Control = FVoxelGenerationConsumerKey::Section(701, FGuid::NewGuid(), 1, {}, 1);
				TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Terrain;
				TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Runtime;
				FString Error;
				if (!Test.TestTrue(TEXT("两个真实原账本在根构造前接纳"),
					FVoxelTerrainResources::Create({}, Control, Terrain, Error) && FVoxelTerrainResources::Create({}, Control, Runtime, Error))) return true;
				if (!Test.TestTrue(TEXT("Scheduler 完整控制根预授予"),
					FVoxelTaskScheduler::Create(Terrain.ToSharedRef(), Runtime.ToSharedRef(), Control, Scheduler, Error))) return true;
				if (!Test.TestTrue(TEXT("初始化真实原 Scheduler 账本"), Scheduler->InitializeStatistics(701))) return true;
				State = MakeShared<FNativeBackingState, ESPMode::ThreadSafe>(Scheduler->GetTerrainResources());
				FVoxelTaskBudget Budget;
				Budget.MaxConcurrentTasks = 1;
				Scheduler->SetBudget(Budget);
				Consumer = FVoxelGenerationConsumerKey::Section(701, FGuid::NewGuid(), 1, FIntVector(7, 8, 9), 1);
				if (!Queue(false, 1)) return true;
				NativeHold = FVoxelSchedulerNativeBackingTestAccess::HoldNative(*Scheduler);
				InitialReceipt = FVoxelSchedulerNativeBackingTestAccess::Receipt(*Scheduler);
				InitialReservation = FVoxelSchedulerNativeBackingTestAccess::ExecutionReservation(*Scheduler);
				SetPhase(EBackingPhase::HeldNative);
			}
			if (Phase != EBackingPhase::Drain && FPlatformTime::Seconds() > Deadline)
			{
				Test.AddError(TEXT("Scheduler 原生账本回归阶段超时"));
				CancelAndDrain();
			}
			Scheduler->Tick([](FVoxelTaskResult&&) {}, 5.0);
			switch (Phase)
			{
			case EBackingPhase::HeldNative:
				if (!NativeHold.IsCompleted()) return false;
				Test.TestEqual(TEXT("engine 引用尾部尚存时不交付结果"), Scheduler->ActiveCount(), 1);
				Test.TestTrue(TEXT("engine 引用尾部保留真实 Working 授额"),
					State->Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)] > 0);
				NativeHold = UE::Tasks::FTask();
				SetPhase(EBackingPhase::Continuation);
				return false;
			case EBackingPhase::Continuation:
				if (Scheduler->ActiveCount() || Scheduler->GetDiagnostics().Pending != 1) return false;
				Test.TestEqual(TEXT("续跑保留同一个统计执行身份"), FVoxelSchedulerNativeBackingTestAccess::Receipt(*Scheduler), InitialReceipt);
				Test.TestTrue(TEXT("续跑保留同一个真实控制对象"),
					FVoxelSchedulerNativeBackingTestAccess::ExecutionReservation(*Scheduler) == InitialReservation);
				Test.TestEqual(TEXT("续跑不复制任何 Execute 捕获"), State->Copies.Load(), 0);
				{
					const auto BlockerConsumer = FVoxelGenerationConsumerKey::Section(701, FGuid::NewGuid(), 1, {}, 9);
					if (!Test.TestTrue(TEXT("外部压力先授予两个真实句柄"),
						State->Resources->Acquire(BlockerConsumer, EVoxelTerrainResourcePool::Working, 1, Blocker) &&
						State->Resources->Acquire(BlockerConsumer, EVoxelTerrainResourcePool::Input, 1, InputBlocker)))
					{
						CancelAndDrain();
						return false;
					}
					const auto Snapshot = State->Resources->Snapshot();
					if (!Test.TestTrue(TEXT("外部压力同时占满 Input 和 Working"),
						Blocker->Resize(Blocker->Bytes() + Snapshot.Limits.WorkingBytes - Snapshot.Used[int32(EVoxelTerrainResourcePool::Working)]) &&
						InputBlocker->Resize(InputBlocker->Bytes() + Snapshot.Limits.InputBytes - Snapshot.Used[int32(EVoxelTerrainResourcePool::Input)])))
					{
						CancelAndDrain();
						return false;
					}
				}
				State->PressureExpected.Store(true);
				State->AllowContinuation.Store(true);
				Scheduler->Tick([](FVoxelTaskResult&&) {}, 5.0);
				Test.TestTrue(TEXT("账本无空余仍可启动原续跑"), Scheduler->ActiveCount() > 0);
				SetPhase(EBackingPhase::CapacityWait);
				return false;
			case EBackingPhase::CapacityWait:
				if (Applied == 0)
				{
					Test.TestTrue(TEXT("压力下每一步都复用原控制对象"),
						FVoxelSchedulerNativeBackingTestAccess::ExecutionReservation(*Scheduler) == InitialReservation);
					return false;
				}
				Test.TestTrue(TEXT("三次续跑期间两本原账本始终没有空余"), State->PressureObserved.Load());
				State->PressureExpected.Store(false);
				InputBlocker.Reset();
				Blocker.Reset();
				SetPhase(EBackingPhase::Finished);
				return false;
			case EBackingPhase::Finished:
				if (Scheduler->ActiveCount() || Scheduler->GetDiagnostics().Pending || Applied == 0) return false;
				Test.TestEqual(TEXT("无需外部释放即可完成四步原 Execute"), State->Executions.Load(), 4);
				Test.TestEqual(TEXT("最终 Apply 只执行一次"), Applied, 1);
				Test.TestEqual(TEXT("从绑定到原 attempt 完成都没有捕获复制"), State->Copies.Load(), 0);
				Test.TestTrue(TEXT("真实执行前 Input 和 native Working 已授额"), State->GrantObserved.Load());
				Test.TestTrue(TEXT("续跑沿用完整原 consumer"), State->ConsumerObserved.Load());
				Test.TestEqual(TEXT("结果和原生 task 退休后归还 Working"),
					State->Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], 0ull);
				Consumer.RequestSerial = 2;
				if (!Queue(true, 2)) return true;
				SetPhase(EBackingPhase::CancelEntered);
				return false;
			case EBackingPhase::CancelEntered:
				if (!State->Entered.Load()) return false;
				Scheduler->CancelMatching([](EVoxelTaskKind, const FVoxelTaskStamp& InStamp) { return InStamp.Token == 2; });
				Test.TestTrue(TEXT("取消仍保留实际 reader 的 Working"),
					State->Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)] > 0);
				State->Release->Trigger();
				SetPhase(EBackingPhase::Canceled);
				return false;
			case EBackingPhase::Canceled:
				if (Scheduler->ActiveCount() || Scheduler->GetDiagnostics().Pending || Applied < 2) return false;
				Test.TestTrue(TEXT("运行期间 Cancel 对实际 Execute 可见"), State->CancelObserved.Load());
				Test.TestTrue(TEXT("Execute 即使返回成功，取消回执仍被拒收"), bCanceledApplied);
				Test.TestEqual(TEXT("取消的全部 native backing 在读者退出后退休"),
					State->Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], 0ull);
				return true;
			case EBackingPhase::Drain:
				return Scheduler->ActiveCount() == 0 && Scheduler->GetDiagnostics().Pending == 0;
			default:
				return false;
			}
		}

	private:
		bool Queue(const bool bInBlock, const uint64 InToken)
		{
			FVoxelTaskRequest Request;
			Request.GenerationConsumer = Consumer;
			Request.Kind = EVoxelTaskKind::BuildGenerationPlan;
			Request.WorkClass = EVoxelWorkClass::Critical;
			Request.Stamp.WorldEpoch = Consumer.WorldEpoch;
			Request.Stamp.OwnerId = Consumer.OwnerId;
			Request.Stamp.OwnerIncarnation = Consumer.OwnerIncarnation;
			Request.Stamp.Section = Consumer.Min;
			Request.Stamp.Token = InToken;
			Request.bResumable = !bInBlock;
			FString Error;
			if (!Request.BindExecute(State->Resources, Consumer, FCountingExecute(State.ToSharedRef(), Consumer, bInBlock), Error) ||
				!Request.BindGetBlockReason(State->Resources, Consumer, [SharedState = State, bInBlock]()
				{
					return !bInBlock && SharedState->Executions.Load() >= 1 && !SharedState->AllowContinuation.Load() ?
						EVoxelTaskBlockReason::Dependency : EVoxelTaskBlockReason::None;
				}, Error) || !Request.BindApply(State->Resources, Consumer, [this](FVoxelTaskResult&& InResult)
				{
					++Applied;
					if (InResult.Stamp.Token == 2) bCanceledApplied = InResult.bCanceled && !InResult.bSuccess;
				}, Error))
			{
				Test.AddError(Error);
				return false;
			}
			return Test.TestTrue(TEXT("真实原账本 Request 接纳"), Scheduler->Enqueue(MoveTemp(Request)));
		}

		void SetPhase(const EBackingPhase InPhase)
		{
			Phase = InPhase;
			Deadline = FPlatformTime::Seconds() + 15.0;
		}

		void CancelAndDrain()
		{
			NativeHold = UE::Tasks::FTask();
			InputBlocker.Reset();
			Blocker.Reset();
			State->PressureExpected.Store(false);
			State->AllowContinuation.Store(true);
			State->Release->Trigger();
			Scheduler->CancelMatching([](EVoxelTaskKind, const FVoxelTaskStamp&) { return true; });
			SetPhase(EBackingPhase::Drain);
		}

		FAutomationTestBase& Test;
		TUniquePtr<FVoxelTaskScheduler> Scheduler;
		TSharedPtr<FNativeBackingState, ESPMode::ThreadSafe> State;
		FVoxelGenerationConsumerKey Consumer;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputBlocker;
		UE::Tasks::FTask NativeHold;
		FGuid InitialReceipt;
		const void* InitialReservation = nullptr;
		EBackingPhase Phase = EBackingPhase::None;
		double Deadline = 0.0;
		int32 Applied = 0;
		bool bCanceledApplied = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTaskSchedulerNativeBackingTest,
	"WHFramework.Voxel.FineRefactor.Scheduler.NativeBackingLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTaskSchedulerNativeBackingTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	ADD_LATENT_AUTOMATION_COMMAND(FNativeBackingCommand(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTaskSchedulerExecutionAdmissionTest,
	"WHFramework.Voxel.FineRefactor.Scheduler.ExecutionAdmissionCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTaskSchedulerExecutionAdmissionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Control = FVoxelGenerationConsumerKey::Section(705, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Terrain;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Runtime;
	TUniquePtr<FVoxelTaskScheduler> Scheduler;
	FString Error;
	if (!TestTrue(TEXT("创建真实原账本与生产 Scheduler"),
		FVoxelTerrainResources::Create({}, Control, Terrain, Error) &&
		FVoxelTerrainResources::Create({}, Control, Runtime, Error) &&
		FVoxelTaskScheduler::Create(Terrain.ToSharedRef(), Runtime.ToSharedRef(), Control, Scheduler, Error) &&
		Scheduler->InitializeStatistics(705)))
	{
		return false;
	}
	FVoxelTaskBudget Budget;
	Budget.MaxPendingTasks = 1;
	Budget.MaxConcurrentTasks = 1;
	Scheduler->SetBudget(Budget);
	const auto Owner = FGuid::NewGuid();
	int32 Executed = 0;
	int32 Canceled = 0;
	const auto MakeRequest = [&](const uint64 Token, const EVoxelWorkClass WorkClass)
	{
		FVoxelTaskRequest Request;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(705, Owner, 1, {}, Token);
		Request.GenerationConsumer = Consumer;
		Request.Kind = EVoxelTaskKind::BuildGenerationPlan;
		Request.WorkClass = WorkClass;
		Request.Stamp.WorldEpoch = Consumer.WorldEpoch;
		Request.Stamp.OwnerId = Consumer.OwnerId;
		Request.Stamp.OwnerIncarnation = Consumer.OwnerIncarnation;
		Request.Stamp.Token = Token;
		TestTrue(TEXT("在函数分配前授予回调容量"),
			Request.BindExecute(Terrain.ToSharedRef(), Consumer, [&Executed](const TAtomic<bool>&)
			{
				++Executed;
				FVoxelTaskResult Result;
				Result.bSuccess = true;
				return Result;
			}, Error) && Request.BindGetBlockReason(Terrain.ToSharedRef(), Consumer, []()
			{
				return EVoxelTaskBlockReason::Dependency;
			}, Error) && Request.BindApply(Terrain.ToSharedRef(), Consumer, [&Canceled](FVoxelTaskResult&& Result)
			{
				if (Result.bCanceled)
				{
					++Canceled;
				}
			}, Error));
		return Request;
	};
	auto Original = MakeRequest(1, EVoxelWorkClass::Background);
	const auto OriginalStamp = Original.Stamp;
	if (!TestTrue(TEXT("被依赖阻塞的原请求在接纳时已有执行授额"), Scheduler->Enqueue(MoveTemp(Original))))
	{
		return false;
	}
	TestTrue(TEXT("尚未 Launch 即保留真实 Working"), Terrain->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)] > 0);
	const void* OriginalReservation = FVoxelSchedulerNativeBackingTestAccess::ExecutionReservation(*Scheduler);
	auto Replacement = MakeRequest(2, EVoxelWorkClass::Critical);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	const auto PressureConsumer = FVoxelGenerationConsumerKey::Section(705, FGuid::NewGuid(), 1, {}, 9);
	if (!TestTrue(TEXT("外部压力占满原 Working"), Terrain->Acquire(PressureConsumer,
		EVoxelTerrainResourcePool::Working, 1, Pressure)))
	{
		return false;
	}
	const auto BeforeFill = Terrain->Snapshot();
	if (!TestTrue(TEXT("完成真实 Working 压力填充"), Pressure->Resize(Pressure->Bytes() +
		BeforeFill.Limits.WorkingBytes - BeforeFill.Used[int32(EVoxelTerrainResourcePool::Working)])))
	{
		return false;
	}
	TestFalse(TEXT("新请求执行容量不足在接纳点拒收"), Scheduler->Enqueue(MoveTemp(Replacement)));
	TestTrue(TEXT("拒收保留原请求与原控制对象"), Scheduler->Has(OriginalStamp, EVoxelTaskKind::BuildGenerationPlan) &&
		FVoxelSchedulerNativeBackingTestAccess::ExecutionReservation(*Scheduler) == OriginalReservation);
	TestTrue(TEXT("拒收仍可重试原新请求回调"), bool(Replacement.Execute));
	TestEqual(TEXT("执行授额拒收明确报告 Working 阻塞"), Replacement.Blocked, EVoxelTaskBlockReason::Memory);
	TestEqual(TEXT("拒收不取消旧请求"), Canceled, 0);
	Pressure.Reset();
	auto InputDenied = MakeRequest(4, EVoxelWorkClass::Critical);
	if (!TestTrue(TEXT("输入压力前保留完整接纳尝试"), Scheduler->ObserveDemand(InputDenied.Kind,
		InputDenied.Stamp, EVoxelTaskBlockReason::InputMemory)))
	{
		return false;
	}
	if (!TestTrue(TEXT("外部输入压力句柄预授予"), Terrain->Acquire(PressureConsumer,
		EVoxelTerrainResourcePool::Input, 1, Pressure)))
	{
		return false;
	}
	const auto BeforeInputFill = Terrain->Snapshot();
	if (!TestTrue(TEXT("外部压力占满原 Input"), Pressure->Resize(Pressure->Bytes() +
		BeforeInputFill.Limits.InputBytes - BeforeInputFill.Used[int32(EVoxelTerrainResourcePool::Input)])))
	{
		return false;
	}
	TestFalse(TEXT("控制对象输入不足也在接纳点拒收"), Scheduler->Enqueue(MoveTemp(InputDenied)));
	TestEqual(TEXT("输入拒收明确报告 Input 阻塞"), InputDenied.Blocked, EVoxelTaskBlockReason::InputMemory);
	TestTrue(TEXT("输入拒收保留原任务身份与回调"), Scheduler->Has(OriginalStamp, EVoxelTaskKind::BuildGenerationPlan) &&
		FVoxelSchedulerNativeBackingTestAccess::ExecutionReservation(*Scheduler) == OriginalReservation && bool(InputDenied.Execute));
	TestEqual(TEXT("输入拒收也不先取消旧请求"), Canceled, 0);
	Pressure.Reset();
	if (!TestTrue(TEXT("外部释放后原新请求正常接纳"), Scheduler->Enqueue(MoveTemp(Replacement))))
	{
		return false;
	}
	Scheduler->Tick([](FVoxelTaskResult&&) {}, 5.0);
	TestEqual(TEXT("仅成功接纳替换后才交付旧取消"), Canceled, 1);
	TestTrue(TEXT("新排队请求仍保留原执行授额"), Terrain->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)] > 0);
	Scheduler->CancelMatching([](EVoxelTaskKind, const FVoxelTaskStamp&) { return true; });
	Scheduler->Tick([](FVoxelTaskResult&&) {}, 5.0);
	TestEqual(TEXT("排队取消只交付一次"), Canceled, 2);
	TestEqual(TEXT("真实排队控制对象销毁后归还 Working"), Terrain->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], 0ull);
	auto StopRequest = MakeRequest(3, EVoxelWorkClass::Critical);
	if (!TestTrue(TEXT("停止前接纳新的完整执行授额"), Scheduler->Enqueue(MoveTemp(StopRequest))))
	{
		return false;
	}
	Scheduler->StopAndJoin();
	TestEqual(TEXT("StopAndJoin 退休所有尚未 Launch 的授额"), Terrain->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], 0ull);
	TestEqual(TEXT("依赖阻塞期间从未偷偷执行生产回调"), Executed, 0);
	TestEqual(TEXT("停止后无排队请求"), Scheduler->GetDiagnostics().Pending, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTaskSchedulerBootstrapTest,
	"WHFramework.Voxel.FineRefactor.Scheduler.BootstrapCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTaskSchedulerBootstrapTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Control = FVoxelGenerationConsumerKey::Section(703, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Terrain;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Runtime;
	FString Error;
	if (!TestTrue(TEXT("原 Terrain ledger 根预授予"), FVoxelTerrainResources::Create({}, Control, Terrain, Error)) ||
		!TestTrue(TEXT("原 Runtime ledger 根预授予"), FVoxelTerrainResources::Create({}, Control, Runtime, Error))) return false;
	const uint64 LedgerRoot = VoxelBoundedAllocation::SharedBytes<FVoxelTerrainResources>();
	TestEqual(TEXT("账本根含真实 intrusive controller 布局"), Terrain->Snapshot().RootInputBytes, LedgerRoot);
	TestEqual(TEXT("首次对象构造即有原 Input 基数"), Terrain->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], LedgerRoot);
	FVoxelTerrainResourceLimits TooSmall;
	TooSmall.InputBytes = LedgerRoot - 1;
	FVoxelTerrainResources* OriginalTerrain = Terrain.Get();
	const uint64 BeforeRootDenial = Terrain->Snapshot().Version;
	TestFalse(TEXT("根下限不可达则拒绝构造替换"), FVoxelTerrainResources::Create(TooSmall, Control, Terrain, Error));
	TestTrue(TEXT("账本根拒收保留原 Out"), Terrain.Get() == OriginalTerrain);
	TestEqual(TEXT("账本根拒收不改变原版本"), Terrain->Snapshot().Version, BeforeRootDenial);
	TestFalse(TEXT("Configure 不能削掉不可释放根"), Terrain->Configure(TooSmall));
	TestEqual(TEXT("Configure 拒收保留原 Input"), Terrain->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], LedgerRoot);
	TUniquePtr<FVoxelTaskScheduler> Scheduler;
	if (!TestTrue(TEXT("控制根在 Scheduler new 前授额"), FVoxelTaskScheduler::Create(
		Terrain.ToSharedRef(), Runtime.ToSharedRef(), Control, Scheduler, Error))) return false;
	const uint64 SchedulerRoot = FVoxelSchedulerNativeBackingTestAccess::RootInputBytes(*Scheduler);
	TestTrue(TEXT("根包含完整诊断、选择器与分配头"), SchedulerRoot >= sizeof(FVoxelTaskScheduler));
	TestEqual(TEXT("原 consumer 控制根只有一笔租约"), Terrain->Snapshot().LeaseCount, 1);
	TestEqual(TEXT("Input 无重复统计根"), Terrain->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)],
		LedgerRoot + SchedulerRoot + Terrain->Snapshot().LedgerMetadataBytes + Terrain->Snapshot().LeaseHandleBytes);
	TestFalse(TEXT("统计初始化不能切换世界"), Scheduler->InitializeStatistics(704));
	FVoxelTaskScheduler* OriginalScheduler = Scheduler.Get();
	const auto BlockerOwner = FVoxelGenerationConsumerKey::Section(703, FGuid::NewGuid(), 1, {}, 3);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!TestTrue(TEXT("不同 consumer 创建 blocker"), Terrain->Acquire(BlockerOwner, EVoxelTerrainResourcePool::Input, 1, Blocker))) return false;
	const auto BeforeFill = Terrain->Snapshot();
	if (!TestTrue(TEXT("占满 Input 模拟外部容量压力"), Blocker->Resize(Blocker->Bytes() +
		BeforeFill.Limits.InputBytes - BeforeFill.Used[int32(EVoxelTerrainResourcePool::Input)]))) return false;
	const uint64 BeforeSchedulerDenial = Terrain->Snapshot().Version;
	TestFalse(TEXT("旧新 Scheduler 控制根峰值先拒收"), FVoxelTaskScheduler::Create(
		Terrain.ToSharedRef(), Runtime.ToSharedRef(), Control, Scheduler, Error));
	TestTrue(TEXT("控制根拒收保留旧 Out"), Scheduler.Get() == OriginalScheduler);
	TestEqual(TEXT("控制根拒收不先分配或退款"), Terrain->Snapshot().Version, BeforeSchedulerDenial);
	Blocker.Reset();
	Scheduler.Reset();
	const auto AfterDelete = Terrain->Snapshot();
	TestEqual(TEXT("真实 Scheduler delete 后才退控制根"), AfterDelete.LeaseCount, 0);
	TestEqual(TEXT("删除后保留 ledger 原根与历史表"), AfterDelete.Used[int32(EVoxelTerrainResourcePool::Input)],
		AfterDelete.RootInputBytes + AfterDelete.LedgerMetadataBytes);
	if (!TestTrue(TEXT("空账本 Configure 释放历史表"), Terrain->Configure({}))) return false;
	TestEqual(TEXT("历史表释放仍保留 ledger 原根"), Terrain->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], LedgerRoot);
	return true;
}

namespace
{
	template<typename Signature>
	struct TGrantedCallbackBundle
	{
		TGrantedCallbackBundle(TFunction<Signature>&& InFunction,
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
			: Lease(MoveTemp(InLease)), Function(MoveTemp(InFunction))
		{
		}

		TGrantedCallbackBundle(const TGrantedCallbackBundle&) = default;
		TGrantedCallbackBundle(TGrantedCallbackBundle&&) = default;

		~TGrantedCallbackBundle()
		{
			Function = nullptr;
			Lease.Reset();
		}

		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TFunction<Signature> Function;
	};

	struct FWrapperExecute
	{
		explicit FWrapperExecute(int32& InExecutions)
			: Executions(&InExecutions)
		{
		}

		FVoxelTaskResult operator()(const TAtomic<bool>&) const
		{
			++*Executions;
			FVoxelTaskResult Result;
			Result.bSuccess = true;
			return Result;
		}

		int32* Executions = nullptr;
		uint8 Payload[512] = {};
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTaskSchedulerCallbackWrapperTest,
	"WHFramework.Voxel.FineRefactor.Scheduler.CallbackWrapperLease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTaskSchedulerCallbackWrapperTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 64 * 1024;
	if (!TestTrue(TEXT("配置真实 wrapper 原账本"), Resources->Configure(Limits))) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(702, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTaskRequest Request;
	FString Error;
	int32 Executions = 0;
	int32 WrappedExecutions = 0;
	if (!TestTrue(TEXT("原函数先授额再绑定"), Request.BindExecute(Resources, Consumer, FWrapperExecute(Executions), Error))) return false;
	const uint64 OriginalBytes = Request.GetBindingBytes();
	const uint64 OriginalSerial = Request.ExecuteBindingLease->Serial();
	TWeakPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OriginalLease = Request.ExecuteBindingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto BlockerConsumer = FVoxelGenerationConsumerKey::Section(702, FGuid::NewGuid(), 1, {}, 2);
	if (!TestTrue(TEXT("外部 blocker 创建原 Input 句柄"), Resources->Acquire(BlockerConsumer,
		EVoxelTerrainResourcePool::Input, 1, Blocker))) return false;
	const auto BeforeFill = Resources->Snapshot();
	if (!TestTrue(TEXT("外部 blocker 占满 Input"), Blocker->Resize(Blocker->Bytes() +
		Limits.InputBytes - BeforeFill.Used[int32(EVoxelTerrainResourcePool::Input)]))) return false;
	bool bFactoryCalled = false;
	const auto Factory = [&bFactoryCalled, &WrappedExecutions](TFunction<FVoxelTaskResult(const TAtomic<bool>&)>&& Original,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease)
	{
		bFactoryCalled = true;
		return [Previous = TGrantedCallbackBundle<FVoxelTaskResult(const TAtomic<bool>&)>(MoveTemp(Original), MoveTemp(Lease)),
			&WrappedExecutions](const TAtomic<bool>& Cancel)
		{
			++WrappedExecutions;
			return Previous.Function(Cancel);
		};
	};
	const auto BeforeDenied = Resources->Snapshot();
	TestFalse(TEXT("容量拒收不能先 move 原函数"), Request.WrapExecute(Resources, Consumer, Factory, Error));
	TestFalse(TEXT("容量拒收尚未调用 factory"), bFactoryCalled);
	TestEqual(TEXT("拒收保持原租约身份"), Request.ExecuteBindingLease->Serial(), OriginalSerial);
	TestEqual(TEXT("拒收保持原绑定链 bytes"), Request.GetBindingBytes(), OriginalBytes);
	TestEqual(TEXT("拒收不改变真实账本版本"), Resources->Snapshot().Version, BeforeDenied.Version);
	TAtomic<bool> Cancel{false};
	TestTrue(TEXT("拒收后原函数仍可消费"), Request.Execute(Cancel).bSuccess);
	Blocker.Reset();
	if (!TestTrue(TEXT("外部释放后 wrapper 正常先授额"), Request.WrapExecute(Resources, Consumer, Factory, Error))) return false;
	TestTrue(TEXT("wrapper 捕获仍持原绑定租约"), OriginalLease.IsValid());
	TestEqual(TEXT("第一层 wrapper 保留旧新两份句柄"), Request.GetBindingLeaseCount(), 2);
	if (!TestTrue(TEXT("第二层 wrapper 沿原 consumer 叠加"), Request.WrapExecute(Resources, Consumer, Factory, Error))) return false;
	TestEqual(TEXT("第二层 wrapper 聚合全部三份句柄"), Request.GetBindingLeaseCount(), 3);
	TestTrue(TEXT("多层 wrapper 调用原函数"), Request.Execute(Cancel).bSuccess);
	TestEqual(TEXT("原函数只由实际调用执行"), Executions, 2);
	TestEqual(TEXT("两层 wrapper 各执行一次"), WrappedExecutions, 2);
	TestTrue(TEXT("最内层原租约保持到完整链退休"), OriginalLease.IsValid());
	int32 Applies = 0;
	if (!TestTrue(TEXT("Apply 绑定"), Request.BindApply(Resources, Consumer,
		[&Applies](FVoxelTaskResult&&) { ++Applies; }, Error))) return false;
	if (!TestTrue(TEXT("Apply wrapper 同样保留原租约"), Request.WrapApply(Resources, Consumer,
		[](TFunction<void(FVoxelTaskResult&&)>&& Original, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease)
		{
			return [Previous = TGrantedCallbackBundle<void(FVoxelTaskResult&&)>(MoveTemp(Original), MoveTemp(Lease))](FVoxelTaskResult&& Result)
			{
				Previous.Function(MoveTemp(Result));
			};
		}, Error))) return false;
	Request.Apply(FVoxelTaskResult());
	TestEqual(TEXT("Apply wrapper 仍只交付原函数一次"), Applies, 1);
	Request = FVoxelTaskRequest();
	TestFalse(TEXT("捕获物理销毁后最内层原租约才退休"), OriginalLease.IsValid());
	return true;
}
#endif
