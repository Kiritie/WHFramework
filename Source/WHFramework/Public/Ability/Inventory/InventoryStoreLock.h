#pragma once

#include "CoreMinimal.h"

// A non-recursive process lease. Its handle may move between worker and game threads.
// Windows must not keep a child file open while retiring a store or its parent world.
class WHFRAMEWORK_API FInventoryStoreLock
{
public:
    static int64 Acquire(const FString& InDirectory, FString& OutError);
    static void Release(int64 InHandle);
};
