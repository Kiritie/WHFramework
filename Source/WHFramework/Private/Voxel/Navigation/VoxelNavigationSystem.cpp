#include "Voxel/Navigation/VoxelNavigationSystem.h"

#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Navigation/VoxelNavigationData.h"

UVoxelNavigationSystem::UVoxelNavigationSystem(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// 基类构造函数注册的非virtual回调会在GT等待整个路径批次。
		// 只移除本实例的该项订阅；下一次Tick用真实Task完成状态继续。
		FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);
	}
}

bool UVoxelNavigationSystem::IsTerrainQueryBatchRunning() const
{
	return AsyncPathFindingTask.GetReference() && !AsyncPathFindingTask->IsComplete();
}

void UVoxelNavigationSystem::HoldAsyncOwners()
{
	if (!AsyncOwner.IsValid()) AsyncOwner.Reset(this);
	if (!AsyncWorld.IsValid()) AsyncWorld.Reset(GetWorld());
}

void UVoxelNavigationSystem::ReleaseAsyncOwners()
{
	check(!IsTerrainQueryBatchRunning());
	AsyncWorld.Reset();
	AsyncOwner.Reset();
}

void UVoxelNavigationSystem::Tick(const float InDeltaSeconds)
{
	check(IsInGameThread());
	if (bDispatching) return;
	if (bCleanupRequested)
	{
		PumpDeferredCleanup(InDeltaSeconds);
		return;
	}
	// 原生Find返回之后，批次仍会追加Completed数组；不能仅检查NavData的局部读计数。
	if (IsTerrainQueryBatchRunning()) return;
	AsyncPathFindingTask.SafeRelease();
	TArray<FAsyncPathFindingQuery> Completed;
	Swap(Completed, AsyncPathFindingCompletedQueries);
	HoldAsyncOwners();
	ON_SCOPE_EXIT
	{
		if (!IsTerrainQueryBatchRunning() && !bCleanupRequested) ReleaseAsyncOwners();
	};
	{
		// 先派发旧批次，再允许父类启动新批次，避免同路径的新worker与旧回调同时填充/读取。
		TGuardValue<bool> DispatchGuard(bDispatching, true);
		for (const FAsyncPathFindingQuery& Query : Completed)
		{
			if (bCleanupRequested) break;
			ENavigationQueryResult::Type Result = Query.Result.Result;
			if (auto* Data = const_cast<AVoxelNavigationData*>(Cast<AVoxelNavigationData>(Query.NavData.Get())))
			{
				Result = Data->ValidateAsyncResult(Query);
				Data->ReleaseAsyncQuery(Query);
			}
			Query.OnDoneDelegate.ExecuteIfBound(Query.QueryID, Result, Query.Result.Path);
		}
	}
	if (bCleanupRequested) return;
	bool bPrepared = true;
	for (FAsyncPathFindingQuery& Query : AsyncPathFindingQueries)
	{
		if (auto* Data = const_cast<AVoxelNavigationData*>(Cast<AVoxelNavigationData>(Query.NavData.Get())))
		{
			bPrepared &= Data->PrepareAsyncQuery(Query);
		}
	}
	// 未获得自己的请求控制容量时，原QueryId仍留在引擎队列，Abort入口仍可找到它。
	if (bPrepared) Super::Tick(InDeltaSeconds);
}

bool UVoxelNavigationSystem::TryCloseTerrainQueries(AVoxelNavigationData& InData)
{
	check(IsInGameThread());
	if (bDispatching || IsTerrainQueryBatchRunning()) return false;
	// 显式世界退出撤回尚未派发的请求，沿用引擎Abort的无回调语义。
	int32 Removed = 0;
	auto Withdraw = [&](TArray<FAsyncPathFindingQuery>& InQueries)
	{
		for (int32 Index = InQueries.Num() - 1; Index >= 0; --Index)
		{
			if (InQueries[Index].NavData.Get() != &InData) continue;
			if (Removed == 32) return false;
			InData.ReleaseAsyncQuery(InQueries[Index]);
			InQueries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			++Removed;
		}
		return true;
	};
	return Withdraw(AsyncPathFindingQueries) && Withdraw(AsyncPathFindingCompletedQueries);
}

void UVoxelNavigationSystem::CleanUp(const FNavigationSystem::ECleanupMode InMode)
{
	check(IsInGameThread());
	if (bCleanUpDone) return;
	CleanupMode = InMode;
	bCleanupRequested = true;
	HoldAsyncOwners();
	if (PumpDeferredCleanup(0.f) && !CleanupTicker.IsValid())
	{
		// World停止Tick之后仍检查实际任务回收；不等待，也不依赖销毁中的World继续Tick。
		CleanupTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UVoxelNavigationSystem::TickDeferredCleanup));
	}
}

bool UVoxelNavigationSystem::PumpDeferredCleanup(const float InDeltaSeconds)
{
	(void)InDeltaSeconds;
	if (!bCleanupRequested) return false;
	if (bDispatching || IsTerrainQueryBatchRunning()) return true;
	for (ANavigationData* Data : NavDataSet)
	{
		if (auto* Terrain = Cast<AVoxelNavigationData>(Data))
		{
			if (!Terrain->CanCleanupNativeTerrain()) return true;
			if (!Terrain->TryCloseQueriesForWorldStop()) return true;
		}
	}
	AsyncPathFindingTask.SafeRelease();
	AsyncPathFindingQueries.Reset();
	AsyncPathFindingCompletedQueries.Reset();
	Super::CleanUp(CleanupMode);
	bCleanupRequested = false;
	ReleaseAsyncOwners();
	if (CleanupTicker.IsValid() && !bCleanupTickerRunning)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CleanupTicker);
		CleanupTicker.Reset();
	}
	return false;
}

bool UVoxelNavigationSystem::TickDeferredCleanup(const float InDeltaSeconds)
{
	TGuardValue<bool> TickerGuard(bCleanupTickerRunning, true);
	const bool bContinue = PumpDeferredCleanup(InDeltaSeconds);
	if (!bContinue) CleanupTicker.Reset();
	return bContinue;
}

bool UVoxelNavigationSystem::IsReadyForFinishDestroy()
{
	if (IsTerrainQueryBatchRunning() || bDispatching) return false;
	if (bCleanupRequested && PumpDeferredCleanup(0.f)) return false;
	return Super::IsReadyForFinishDestroy();
}
