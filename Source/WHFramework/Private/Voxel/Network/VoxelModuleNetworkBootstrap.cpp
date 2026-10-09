#include "Voxel/VoxelModule.h"

#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

bool UVoxelModule::EnsureTerrainFactBootstrap(
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& OutResources,
	FVoxelGenerationConsumerKey& OutControl, FString& OutError)
{
	check(IsInGameThread());
	if (WorldState == EVoxelWorldState::Closing || (!Runtime && Epoch == MAX_uint64) || !WorldGenerationProfile)
	{
		OutError = TEXT("InvalidResource: terrain fact bootstrap world is unavailable");
		return false;
	}
	const auto* Profile = WorldGenerationProfile->View.LoadSynchronous();
	if (!Profile)
	{
		OutError = TEXT("InvalidResource: terrain fact bootstrap requires the original ViewProfile");
		return false;
	}
	const auto& Policy = Profile->Budget;
	if (Policy.MaximumRuntimeFactsInputMegabytes < 16 || Policy.MaximumRuntimeFactsWorkingMegabytes < 32 ||
		Policy.MaximumRuntimeFactsStagedMegabytes < 16 || Policy.MaximumRuntimeFactsResidentMegabytes < 128 ||
		Policy.RuntimeFactsOverlapMegabytes < 16)
	{
		OutError = TEXT("Oversized: terrain fact bootstrap profile limits");
		return false;
	}
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = uint64(Policy.MaximumRuntimeFactsInputMegabytes) * 1024 * 1024;
	Limits.WorkingBytes = uint64(Policy.MaximumRuntimeFactsWorkingMegabytes) * 1024 * 1024;
	Limits.StagedBytes = uint64(Policy.MaximumRuntimeFactsStagedMegabytes) * 1024 * 1024;
	Limits.ResidentBytes = uint64(Policy.MaximumRuntimeFactsResidentMegabytes) * 1024 * 1024;
	Limits.OverlapBytes = uint64(Policy.RuntimeFactsOverlapMegabytes) * 1024 * 1024;
	const uint64 ExpectedEpoch = Runtime ? Epoch : Epoch + 1;
	const auto Existing = Scheduler ? TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>(Scheduler->GetRuntimeResources()) :
		PendingTerrainFactResources;
	if (Existing)
	{
		const auto Control = Existing->GetControlConsumer();
		const auto ExistingLimits = Existing->Snapshot().Limits;
		if (!Control.IsValid() || Control.WorldEpoch != ExpectedEpoch ||
			(!Scheduler && Control != PendingTerrainFactControl) ||
			ExistingLimits.InputBytes != Limits.InputBytes || ExistingLimits.WorkingBytes != Limits.WorkingBytes ||
			ExistingLimits.StagedBytes != Limits.StagedBytes || ExistingLimits.ResidentBytes != Limits.ResidentBytes ||
			ExistingLimits.OverlapBytes != Limits.OverlapBytes)
		{
			OutError = TEXT("InvalidResource: frozen terrain fact bootstrap epoch or limits changed");
			return false;
		}
		OutResources = Existing;
		OutControl = Control;
		OutError.Reset();
		return true;
	}
	if (!PendingTerrainFactControl.IsValid())
		PendingTerrainFactControl = FVoxelGenerationConsumerKey::Section(ExpectedEpoch, FGuid::NewGuid(), 1, {}, 1);
	if (PendingTerrainFactControl.WorldEpoch != ExpectedEpoch)
	{
		OutError = TEXT("InvalidResource: frozen terrain fact bootstrap control changed");
		return false;
	}
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Created;
	if (!FVoxelTerrainResources::Create(Limits, PendingTerrainFactControl, Created, OutError)) return false;
	PendingTerrainFactResources = Created;
	OutResources = MoveTemp(Created);
	OutControl = PendingTerrainFactControl;
	OutError.Reset();
	return true;
}
