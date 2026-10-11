#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

class FVoxelTerrainDomainPlanner;

class WHFRAMEWORK_API FVoxelInterestManager
{
public:
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer)
	{
		Resources = InResources;
		Consumer = InConsumer;
	}
	static void SortExactDemands(FVoxelInterestSet& InOutInterest);
	static bool RefreshPriorities(TConstArrayView<FVoxelStreamingSource> InSources,
		int32 InP0RadiusCells, FVoxelInterestSet& InOutInterest, FString& OutError,
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources = nullptr,
		const FVoxelGenerationConsumerKey& InConsumer = {});
	bool CopyForPriority(const FVoxelInterestSet& InPrevious, FVoxelInterestSet& OutInterest, FString& OutError) const;
	bool BuildPublicationInputs(FVoxelInterestSet& InOutInterest, FString& OutError) const;
	FVoxelInterestSet Compute(
		TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest,
		const FVoxelViewSettings& InViewSettings,
		const FVoxelInterestSet* InPrevious = nullptr) const;

	void BuildSourceExact(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest, const FVoxelInterestSet* InPrevious = nullptr,
		const FVoxelFineResidency* InFineResidency = nullptr) const;
	bool BuildFineShardDemand(const FVoxelStreamingSource& InSource,
		const FVoxelFineResidency& InResidency, FVoxelInterestSet& InOutInterest, FString& OutError) const;
	bool BuildSimulationDemand(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, FVoxelInterestSet& InOutInterest, FString& OutError) const;
	void BuildSourceProxy(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest) const;
	void BuildSourceSurface(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest) const;
	void BuildSourceMacro(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest) const;
	FVoxelInterestSet Finalize(TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet InPrepared, const FVoxelInterestSet* InPrevious = nullptr,
		bool bInRebuildCoarse = true, FVoxelTerrainDomainPlanner* InPersistentCoarse = nullptr) const;

private:
	bool ReserveFinalization(FVoxelInterestSet& InOutInterest, int32 InFineCount, FString& OutError) const;
	static bool SettleFinalization(FVoxelInterestSet& InOutInterest, FString& OutError);
	bool ReserveDemands(FVoxelInterestSet& InOutInterest, FVoxelSourceInterest& InOutSource,
		int32 InAdditionalKeys, FString& OutError) const;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;

	void AddExactSource(
		const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest,
		const FVoxelViewSettings& InViewSettings,
		FVoxelInterestSet& InOutInterest,
		const FVoxelInterestSet* InPrevious, const FVoxelFineResidency* InFineResidency) const;
	void AddViewSource(
		const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest,
		const FVoxelViewSettings& InViewSettings,
		FVoxelInterestSet& InOutInterest,
		bool bBuildProxy,
		bool bBuildSurface,
		bool bBuildMacro) const;
};
