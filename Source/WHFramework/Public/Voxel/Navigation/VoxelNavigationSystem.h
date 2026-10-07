#pragma once

#include "NavigationSystem.h"
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"
#include "VoxelNavigationSystem.generated.h"

class AVoxelNavigationData;

/** 保留引擎请求Id，在实际派发点核对体素世界与完成尝试；GT不join异步批次。 */
UCLASS(Config=Engine)
class WHFRAMEWORK_API UVoxelNavigationSystem : public UNavigationSystemV1
{
	GENERATED_BODY()

public:
	UVoxelNavigationSystem(const FObjectInitializer& InObjectInitializer);
	virtual void Tick(
		float InDeltaSeconds) override;
	virtual void CleanUp(
		FNavigationSystem::ECleanupMode InMode = FNavigationSystem::ECleanupMode::CleanupUnsafe) override;
	virtual bool IsReadyForFinishDestroy() override;
	bool TryCloseTerrainQueries(
		AVoxelNavigationData& InData);
	bool IsTerrainQueryBatchRunning() const;

private:
	bool PumpDeferredCleanup(
		float InDeltaSeconds);
	bool TickDeferredCleanup(
		float InDeltaSeconds);
	void HoldAsyncOwners();
	void ReleaseAsyncOwners();
	TStrongObjectPtr<UVoxelNavigationSystem> AsyncOwner;
	TStrongObjectPtr<UWorld> AsyncWorld;
	FTSTicker::FDelegateHandle CleanupTicker;
	FNavigationSystem::ECleanupMode CleanupMode = FNavigationSystem::ECleanupMode::CleanupUnsafe;
	bool bCleanupRequested = false;
	bool bDispatching = false;
	bool bCleanupTickerRunning = false;
};
