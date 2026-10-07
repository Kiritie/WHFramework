#include "Voxel/Save/VoxelRegionStore.h"

#include "Misc/Crc.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	class FVoxelRegionOverlayReadLease final : public IVoxelFineOverlayReadLeaseSet
	{
	public:
		explicit FVoxelRegionOverlayReadLease(const FVoxelFineOverlayCapture& InCapture)
			: Epoch(InCapture.WorldEpoch), Bounds(InCapture.Bounds), WorldId(InCapture.Manifest.WorldId),
			  RecipeHash(InCapture.Manifest.RecipeHash), Registry(InCapture.Registry), Resident(InCapture.Resident),
			  ContentRevisions(InCapture.ContentRevisions)
		{
			Reads.Reserve(InCapture.Reads.Num());
			for (const FVoxelRegionReadView& Read : InCapture.Reads)
			{
				check(!Reads.Contains(Read.Section));
				Reads.Add(Read.Section, Read);
			}
		}

		virtual uint64 GetWorldEpoch() const override
		{
			return Epoch;
		}

		virtual bool ValidateCapture(const FVoxelFineOverlayCapture& InCapture, FString& OutError) const override
		{
			if (InCapture.ReadLeaseSet.Get() != this || InCapture.WorldEpoch != Epoch ||
				InCapture.Bounds.Min != Bounds.Min || InCapture.Bounds.Max != Bounds.Max ||
				InCapture.Manifest.WorldId != WorldId || InCapture.Manifest.RecipeHash != RecipeHash ||
				InCapture.Registry != Registry || InCapture.Reads.Num() != Reads.Num() ||
				InCapture.Resident.Num() != Resident.Num() ||
				!InCapture.ContentRevisions.OrderIndependentCompareEqual(ContentRevisions))
			{
				OutError = TEXT("Overlay捕获描述不属于固定读取租约");
				return false;
			}
			TSet<FIntVector> Seen;
			Seen.Reserve(InCapture.Reads.Num());
			for (const FVoxelRegionReadView& Read : InCapture.Reads)
			{
				const FVoxelRegionReadView* Expected = Reads.Find(Read.Section);
				if (!Expected || Seen.Contains(Read.Section) || !SameRead(Read, *Expected) ||
					Read.ExpectedRecordBytes == 0 || !Read.GenerationLease ||
					!Read.GenerationLease->Matches(Read.SourceDirectory) || Resident.Find(Read.Section))
				{
					OutError = TEXT("Overlay读取描述重复、缺失或与驻留记录重叠");
					return false;
				}
				Seen.Add(Read.Section);
			}
			for (const auto& Pair : InCapture.Resident.Sections)
			{
				const FVoxelOverlaySnapshot* Expected = Resident.Sections.Find(Pair.Key);
				if (!Expected || Pair.Value.Section != Pair.Key || Expected->Revision != Pair.Value.Revision ||
					!Expected->Blocks.OrderIndependentCompareEqual(Pair.Value.Blocks) ||
					!Expected->Entities.OrderIndependentCompareEqual(Pair.Value.Entities))
				{
					OutError = TEXT("驻留Overlay不属于捕获的版本");
					return false;
				}
			}
			for (const auto& Pair : InCapture.Resident.PinnedSections)
			{
				const auto* Expected = Resident.PinnedSections.Find(Pair.Key);
				if (!Pair.Value || !Expected || *Expected != Pair.Value || Pair.Value->Section != Pair.Key ||
					InCapture.Resident.Sections.Contains(Pair.Key))
				{
					OutError = TEXT("驻留提交版本与不可变租约身份不符");
					return false;
				}
			}
			OutError.Reset();
			return true;
		}

		virtual bool ValidateRead(const FVoxelRegionReadView& InRead, TConstArrayView<uint8> InBytes,
			uint64 InDecodedRevision, FString& OutError) const override
		{
			const FVoxelRegionReadView* Expected = Reads.Find(InRead.Section);
			if (!Expected || !SameRead(InRead, *Expected) || !InRead.GenerationLease ||
				!InRead.GenerationLease->Matches(InRead.SourceDirectory) ||
				static_cast<uint64>(InBytes.Num()) != InRead.ExpectedRecordBytes ||
				FCrc::MemCrc32(InBytes.GetData(), InBytes.Num()) != InRead.ExpectedRecordCrc ||
				(InRead.ExpectedSectionRevision != 0 && InDecodedRevision != InRead.ExpectedSectionRevision))
			{
				OutError = TEXT("Overlay读取内容与固定文件代、记录CRC或Section版本不符");
				return false;
			}
			OutError.Reset();
			return true;
		}

		virtual uint64 GetMetadataBytes() const override
		{
			uint64 Bytes = sizeof(*this) + Reads.GetAllocatedSize() + Resident.GetAllocatedBytes() + ContentRevisions.GetAllocatedSize();
			for (const auto& Pair : Reads)
			{
				Bytes += Pair.Value.SourceDirectory.GetAllocatedSize();
			}
			return Bytes;
		}

	private:
		static bool SameRead(const FVoxelRegionReadView& First, const FVoxelRegionReadView& Second)
		{
			return First.Section == Second.Section && First.SourceDirectory == Second.SourceDirectory &&
				First.GenerationLease == Second.GenerationLease && First.ExpectedSectionRevision == Second.ExpectedSectionRevision &&
				First.ExpectedRegionRevision == Second.ExpectedRegionRevision && First.ExpectedRecordOffset == Second.ExpectedRecordOffset &&
				First.ExpectedRecordBytes == Second.ExpectedRecordBytes && First.ExpectedRecordCrc == Second.ExpectedRecordCrc;
		}

		uint64 Epoch = 0;
		FVoxelGenerationBounds Bounds;
		FGuid WorldId;
		uint64 RecipeHash = 0;
		TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
		FVoxelOverlaySnapshotSet Resident;
		TMap<FIntVector, uint64> ContentRevisions;
		TMap<FIntVector, FVoxelRegionReadView> Reads;
	};
}

TSharedRef<const IVoxelFineOverlayReadLeaseSet, ESPMode::ThreadSafe> FVoxelRegionStore::CreateOverlayReadLeaseSet(
	const FVoxelFineOverlayCapture& InCapture)
{
	return MakeShared<FVoxelRegionOverlayReadLease, ESPMode::ThreadSafe>(InCapture);
}

uint64 FVoxelRegionStore::OverlayReadLeaseBytes(const int32 InPinnedSections, const int32 InReads,
	const uint64 InReadPathBytes, const int32 InContentRevisions)
{
	return VoxelBoundedAllocation::SharedBytes<FVoxelRegionOverlayReadLease>() +
		VoxelBoundedAllocation::MapBytes<FIntVector, TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>>(InPinnedSections) +
		VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelRegionReadView>(InReads) + InReadPathBytes +
		VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(InContentRevisions);
}
