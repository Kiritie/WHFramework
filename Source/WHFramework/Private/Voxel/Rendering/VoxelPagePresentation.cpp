#include "Voxel/Rendering/VoxelPagePresentation.h"

EVoxelPagePublicationDecision VoxelPagePresentation::Decide(const FVoxelPagePublicationInput& InInput)
{
	if (InInput.bPendingPublication)
	{
		return EVoxelPagePublicationDecision::WaitForPublication;
	}
	if (InInput.Desired.bVisible && (!InInput.bCompleteFamily || !InInput.bAllMeshesReady || !InInput.bNewDependenciesReady))
	{
		return EVoxelPagePublicationDecision::KeepPrevious;
	}
	if (!InInput.Desired.bVisible && InInput.Committed.bCommitted && InInput.Committed.bVisible &&
		!InInput.bOldOwnershipCovered)
	{
		return EVoxelPagePublicationDecision::KeepPrevious;
	}
	if (!InInput.Committed.bCommitted && !InInput.Desired.bVisible && InInput.bLayoutSame)
	{
		return EVoxelPagePublicationDecision::NoChange;
	}
	const bool bPresentationMatches = !InInput.Desired.bVisible || InInput.Desired.bEmpty ||
		InInput.bActorPresentationReady;
	if (InInput.Committed.bCommitted && InInput.Committed.Stamp == InInput.Desired.Stamp &&
		InInput.Committed.bVisible == InInput.Desired.bVisible &&
		InInput.Committed.bEmpty == InInput.Desired.bEmpty && InInput.bLayoutSame && bPresentationMatches)
	{
		return EVoxelPagePublicationDecision::NoChange;
	}
	return EVoxelPagePublicationDecision::Publish;
}
