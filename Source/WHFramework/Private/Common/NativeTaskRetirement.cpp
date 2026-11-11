#include "Common/NativeTaskRetirement.h"

#include "HAL/Event.h"
#include "Misc/ScopeLock.h"
#include "Containers/Ticker.h"

namespace
{
struct FRetiredNativeTask
{
	UE::Tasks::FTask Completion;
	TSharedPtr<FEventRef, ESPMode::ThreadSafe> Finished;
	TSharedPtr<void, ESPMode::ThreadSafe> Lifetime;
};
FCriticalSection RetiredNativeTasksMutex;
TArray<FRetiredNativeTask> RetiredNativeTasks;
FTSTicker::FDelegateHandle RetirementTicker;

bool CollectRetiredNativeTasks(float)
{
	FScopeLock Lock(&RetiredNativeTasksMutex);
	RetiredNativeTasks.RemoveAll([](const FRetiredNativeTask& Entry) { return Entry.Completion.IsCompleted(); });
	if (!RetiredNativeTasks.IsEmpty()) return true;
	RetirementTicker.Reset();
	return false;
}
}

void FNativeTaskRetirement::Track(const UE::Tasks::FTask& InTask, TSharedPtr<void, ESPMode::ThreadSafe> InLifetime)
{
	check(IsInGameThread());
	TSharedPtr<FEventRef, ESPMode::ThreadSafe> Finished = MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset);
	UE::Tasks::FTask Completion = UE::Tasks::Launch(UE_SOURCE_LOCATION,
		FNativeTaskRetirementSignal{Finished, InLifetime}, UE::Tasks::Prerequisites(InTask));
	FScopeLock Lock(&RetiredNativeTasksMutex);
	RetiredNativeTasks.RemoveAll([](const FRetiredNativeTask& Entry) { return Entry.Completion.IsCompleted(); });
	RetiredNativeTasks.Add({MoveTemp(Completion), MoveTemp(Finished), MoveTemp(InLifetime)});
	if (!RetirementTicker.IsValid())
		RetirementTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&CollectRetiredNativeTasks));
}

void FNativeTaskRetirement::Drain()
{
	check(IsInGameThread());
	if (RetirementTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RetirementTicker);
		RetirementTicker.Reset();
	}
	for (;;)
	{
		TArray<FRetiredNativeTask> Pending;
		{
			FScopeLock Lock(&RetiredNativeTasksMutex);
			Pending = MoveTemp(RetiredNativeTasks);
		}
		if (Pending.IsEmpty()) break;
		for (const FRetiredNativeTask& Entry : Pending)
		{
			(*Entry.Finished)->Wait();
			Entry.Completion.Wait();
		}
	}
}
