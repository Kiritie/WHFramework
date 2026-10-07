#include "Voxel/Save/VoxelDeltaTypes.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"

FVoxelEditBatch::~FVoxelEditBatch()
{
	if (ResourceLease && !Sections.IsEmpty())
	{
		auto Retired = VoxelBoundedAllocation::MakeWorkerShared<TArray<FVoxelSectionPatch>>(ResourceLease, MoveTemp(Sections));
	}
}

FVoxelEditBatch& FVoxelEditBatch::operator=(FVoxelEditBatch&& InOther)
{
	if (this == &InOther) return *this;
	FVoxelEditBatch Previous(MoveTemp(*this));
	TransactionId = InOther.TransactionId;
	Sections = MoveTemp(InOther.Sections);
	ResourceLease = MoveTemp(InOther.ResourceLease);
	return *this;
}
