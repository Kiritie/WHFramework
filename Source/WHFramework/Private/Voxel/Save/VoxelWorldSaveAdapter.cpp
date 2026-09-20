#include "Voxel/Save/VoxelWorldSaveAdapter.h"

#include "Voxel/Save/VoxelDeltaCodec.h"

bool FVoxelWorldSaveAdapter::Capture(
	FVoxelWorldRuntime& InRuntime,
	const FVoxelWorldManifest& InManifest,
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	const FVoxelRegionStore& InStore,
	FString& OutError)
{
	check(IsInGameThread());
	if (Active.IsSet() || !InRuntime.IsServer() || !InRegistry)
	{
		OutError = TEXT("Voxel save capture is busy or not authoritative");
		return false;
	}
	FVoxelWorldSaveCapture Capture;
	Capture.TransactionId = FGuid::NewGuid();
	Capture.WorldEpoch = InRuntime.Epoch();
	Capture.Manifest = InManifest;
	Capture.Registry = InRegistry;
	Capture.SourceDirectory = InStore.GetSourceDirectory();
	TArray<FIntVector> Sections = InRuntime.ResidentSections();
	Sections.Sort([](const FIntVector& InA, const FIntVector& InB)
	{
		if (InA.X != InB.X) return InA.X < InB.X;
		if (InA.Y != InB.Y) return InA.Y < InB.Y;
		return InA.Z < InB.Z;
	});
	for (const FIntVector& Key : Sections)
	{
		FVoxelSection* Section = InRuntime.FindSection(Key);
		if (!Section ||
			Section->Status != EVoxelSectionStatus::DataReady ||
			Section->PersistedRevision == Section->CommittedRevision)
		{
			continue;
		}
		if (Section->PinCount.Load() == MAX_int32)
		{
			OutError = TEXT("Voxel section save pin overflow");
			return false;
		}
		FVoxelPersistentSection Persistent;
		Persistent.Section = Key;
		Persistent.Revision = Section->CommittedRevision;
		Persistent.Blocks = Section->Overlay;
		Persistent.Entities = Section->Entities;
		Capture.Sections.Add(MoveTemp(Persistent));
		++Section->PinCount;
	}
	Active = MoveTemp(Capture);
	OutError.Reset();
	return true;
}

bool FVoxelWorldSaveAdapter::WriteCapture(
	const FVoxelWorldSaveCapture& InCapture,
	const FString& InTemporaryGenerationDirectory,
	FString& OutError)
{
	if (!InCapture.Registry || !InCapture.TransactionId.IsValid())
	{
		OutError = TEXT("Invalid voxel save capture");
		return false;
	}
	FVoxelRegionWritePlan Plan;
	Plan.TransactionId = InCapture.TransactionId;
	Plan.SourceDirectory = InCapture.SourceDirectory;
	for (const FVoxelPersistentSection& Section : InCapture.Sections)
	{
		if (Section.IsEmpty())
		{
			FVoxelRegionStore::StageDelete(Plan, Section.Section);
			continue;
		}
		TArray<uint8> Bytes;
		if (!FVoxelDeltaCodec::Encode(InCapture.Manifest, *InCapture.Registry, Section, Bytes) ||
			!FVoxelRegionStore::StageSection(Plan, Section.Section, MoveTemp(Bytes)))
		{
			OutError = TEXT("Cannot encode voxel section delta");
			return false;
		}
	}
	return FVoxelRegionStore::WritePendingRegions(Plan, InTemporaryGenerationDirectory, OutError);
}

void FVoxelWorldSaveAdapter::Complete(
	FVoxelWorldRuntime& InRuntime,
	FVoxelRegionStore& InStore,
	const bool bInSuccess,
	const FString& InCommittedDirectory)
{
	check(IsInGameThread());
	if (!Active.IsSet())
	{
		return;
	}
	if (Active->WorldEpoch == InRuntime.Epoch())
	{
		for (const FVoxelPersistentSection& Persistent : Active->Sections)
		{
			if (bInSuccess)
			{
				InRuntime.MarkCommitted(Persistent.Section, Persistent.Revision);
			}
			if (FVoxelSection* Section = InRuntime.FindSection(Persistent.Section))
			{
				check(Section->PinCount.Load() > 0);
				--Section->PinCount;
			}
		}
		if (bInSuccess)
		{
			InStore.AdvanceSource(InCommittedDirectory);
		}
	}
	Active.Reset();
}
