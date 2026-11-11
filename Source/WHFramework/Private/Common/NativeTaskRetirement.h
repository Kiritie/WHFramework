#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "HAL/Event.h"

struct FNativeTaskRetirementSignal
{
	TSharedPtr<FEventRef, ESPMode::ThreadSafe> Finished;
	TSharedPtr<void, ESPMode::ThreadSafe> Lifetime;
	void operator()() const { (*Finished)->Trigger(); }
};

/** Tracks detached native work until its completion callback has returned. */
class FNativeTaskRetirement
{
public:
	static void Track(const UE::Tasks::FTask& InTask, TSharedPtr<void, ESPMode::ThreadSafe> InLifetime = {});
	static void Drain();
};
