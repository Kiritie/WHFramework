#include "Voxel/WorldObject/VoxelWorldObjectPresenterView.h"

#include "Engine/World.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/WorldObject/VoxelWorldObjectPresenter.h"

FVoxelWorldObjectPresenterView::FVoxelWorldObjectPresenterView()
{
}

FVoxelWorldObjectPresenterView::~FVoxelWorldObjectPresenterView()
{
	Reset();
}

void FVoxelWorldObjectPresenterView::Reset()
{
	for (const auto& Pair : Actors)
	{
		if (AVoxelWorldObjectPresenter* Actor = Pair.Value.Get())
		{
			Actor->Destroy();
		}
	}
	Actors.Reset();
	Sections.Reset();
	DirtySections.Reset();
}

void FVoxelWorldObjectPresenterView::Invalidate(UVoxelModule& InModule, const FVoxelWorldObjectRegistry& InObjects, const FVoxelEditBatch& InBatch, TFunctionRef<bool(const FVoxelWorldObjectInstance&)> InIsActive)
{
	TSet<FIntVector> ChangedSections;
	for (const FVoxelSectionPatch& Patch : InBatch.Sections)
	{
		DirtySections.Add(Patch.Section);
		ChangedSections.Add(Patch.Section);
	}
	for (auto Iterator = Actors.CreateIterator(); Iterator; ++Iterator)
	{
		AVoxelWorldObjectPresenter* Actor = Iterator.Value().Get();
		if (!Actor || !Actor->GetDefinition())
		{
			Iterator.RemoveCurrent();
			continue;
		}
		bool bAffected = false;
		for (const FVoxelWorldObjectPart& Part : Actor->GetDefinition()->Footprint)
		{
			const FVoxelSectionKey Key = VoxelCoord::Section(Iterator.Key() + FVoxelWorldObjectRegistry::Rotate(Part.Offset, Actor->GetObject().Yaw));
			if (ChangedSections.Contains(FIntVector(Key.X, Key.Y, Key.Z)))
			{
				bAffected = true;
				break;
			}
		}
		if (!bAffected)
		{
			continue;
		}
		FVoxelWorldObjectInstance Object;
		FString Error;
		if (!InObjects.Resolve(*InModule.GetRuntime(), Iterator.Key(), Object, Error) || Object.DefinitionId != Actor->GetObject().DefinitionId)
		{
			Actor->Destroy();
			Iterator.RemoveCurrent();
			continue;
		}
		Actor->RefreshState(Object, InIsActive(Object), Actor->IsLightInRange());
	}
}

AVoxelWorldObjectPresenter* FVoxelWorldObjectPresenterView::Find(const FIntVector& InAnchor) const
{
	const TWeakObjectPtr<AVoxelWorldObjectPresenter>* Actor = Actors.Find(InAnchor);
	return Actor ? Actor->Get() : nullptr;
}

void FVoxelWorldObjectPresenterView::Refresh(UVoxelModule& InModule, const FVoxelWorldObjectRegistry& InObjects, const TArray<FVector>& InObservers, TFunctionRef<bool(const FVoxelWorldObjectInstance&)> InIsActive)
{
	if (InObservers.IsEmpty() || InObjects.GetMaxMaterializationRadius() <= 0.)
	{
		Reset();
		return;
	}
	const FVoxelWorldRuntime& World = *InModule.GetRuntime();
	TSet<FIntVector> Resident;
	struct FScanCandidate
	{
		FIntVector Key;
		double DistanceSquared = 0.;
		bool bDirty = false;
	};
	TArray<FScanCandidate> Pending;
	const double RadiusSquared = FMath::Square(InObjects.GetMaxMaterializationRadius());
	for (const FIntVector& Key : World.ResidentSections())
	{
		const FVector Min = FVector(Key) * (VoxelBlock::Size * InModule.BlockSize());
		const FBox Bounds(Min, Min + FVector(VoxelBlock::Size * InModule.BlockSize()));
		double DistanceSquared = TNumericLimits<double>::Max();
		for (const FVector& Observer : InObservers)
		{
			DistanceSquared = FMath::Min(DistanceSquared, Bounds.ComputeSquaredDistanceToPoint(Observer));
		}
		if (DistanceSquared > RadiusSquared)
		{
			continue;
		}
		const FVoxelSection* Data = World.FindSection(Key);
		if (!Data || Data->Status != EVoxelSectionStatus::DataReady)
		{
			continue;
		}
		Resident.Add(Key);
		const FSectionObjects* Cached = Sections.Find(Key);
		if (Cached && Cached->Revision == Data->CommittedRevision && Cached->Token == Data->Stamp.Token && !DirtySections.Contains(Key))
		{
			continue;
		}
		Pending.Add({Key, DistanceSquared, DirtySections.Contains(Key)});
	}
	Pending.Sort([](const FScanCandidate& Left, const FScanCandidate& Right)
	{
		return Left.bDirty != Right.bDirty ? Left.bDirty : Left.DistanceSquared < Right.DistanceSquared;
	});
	const int32 ScanCount = FMath::Min(Pending.Num(), 16);
	for (int32 ScanIndex = 0; ScanIndex < ScanCount; ++ScanIndex)
	{
		const FIntVector Key = Pending[ScanIndex].Key;
		const FVoxelSection* Data = World.FindSection(Key);
		FSectionObjects& Cached = Sections.FindOrAdd(Key);
		Cached.Anchors.Reset();
		for (int32 Index = 0; Index < Data->Blocks.Num(); ++Index)
		{
			const FVoxelBlockState State = Data->Blocks[Index];
			const FVoxelWorldObjectDefinitionRuntime* Definition = InObjects.Find(State.TypeId);
			UVoxelWorldObjectDefinition* Source = Definition ? Definition->Source.Get() : nullptr;
			if (!Source || (Source->Presentation != EVoxelWorldObjectPresentation::PresenterActor && Source->Presentation != EVoxelWorldObjectPresentation::StaticMesh) ||
				FVoxelWorldObjectRegistry::PartState(*Definition, 0, uint8(State.State & 3), State.State) != State)
			{
				continue;
			}
			Cached.Anchors.Add(Key * VoxelBlock::Size + VoxelCoord::Unlinear(uint16(Index)));
		}
		Cached.Revision = Data->CommittedRevision;
		Cached.Token = Data->Stamp.Token;
		DirtySections.Remove(Key);
	}
	for (auto Iterator = DirtySections.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Resident.Contains(*Iterator))
		{
			Iterator.RemoveCurrent();
		}
	}
	for (auto Iterator = Sections.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Resident.Contains(Iterator.Key()))
		{
			Iterator.RemoveCurrent();
		}
	}
	struct FCandidate
	{
		FVoxelWorldObjectInstance Object;
		UVoxelWorldObjectDefinition* Definition = nullptr;
		double DistanceSquared = 0.;
	};
	TArray<FCandidate> Candidates;
	for (const auto& Section : Sections)
	{
		for (const FIntVector& Anchor : Section.Value.Anchors)
		{
			const FVector Location = (FVector(Anchor) + FVector(.5)) * InModule.BlockSize();
			double DistanceSquared = TNumericLimits<double>::Max();
			for (const FVector& Observer : InObservers)
			{
				DistanceSquared = FMath::Min(DistanceSquared, FVector::DistSquared(Location, Observer));
			}
			FVoxelBlockState State;
			if (!World.TryGetBlock(Anchor, State))
			{
				continue;
			}
			const FVoxelWorldObjectDefinitionRuntime* Definition = InObjects.Find(State.TypeId);
			UVoxelWorldObjectDefinition* Source = Definition ? Definition->Source.Get() : nullptr;
			if (!Source || DistanceSquared > FMath::Square(double(Source->MaterializationRadius)))
			{
				continue;
			}
			FCandidate Candidate;
			FString Error;
			if (!InObjects.Resolve(World, Anchor, Candidate.Object, Error))
			{
				continue;
			}
			Candidate.Definition = Source;
			Candidate.DistanceSquared = DistanceSquared;
			Candidates.Add(Candidate);
		}
	}
	Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
	{
		return Left.DistanceSquared < Right.DistanceSquared;
	});
	TSet<FIntVector> Wanted;
	for (int32 Index = 0; Index < FMath::Min(Candidates.Num(), 128); ++Index)
	{
		const FCandidate& Candidate = Candidates[Index];
		Wanted.Add(Candidate.Object.Anchor);
		AVoxelWorldObjectPresenter* Actor = Find(Candidate.Object.Anchor);
		if (Actor && Actor->GetDefinition() != Candidate.Definition)
		{
			Actor->Destroy();
			Actor = nullptr;
		}
		if (!Actor)
		{
			UClass* Class = Candidate.Definition->PresenterClass ? Candidate.Definition->PresenterClass.Get() : AVoxelWorldObjectPresenter::StaticClass();
			FActorSpawnParameters Parameters;
			Parameters.ObjectFlags |= RF_Transient;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = InModule.GetWorld()->SpawnActor<AVoxelWorldObjectPresenter>(Class, FTransform::Identity, Parameters);
			if (!Actor)
			{
				continue;
			}
			Actor->Initialize(Candidate.Definition, Candidate.Object, InModule.BlockSize());
			Actors.Add(Candidate.Object.Anchor, Actor);
		}
		Actor->RefreshState(Candidate.Object, InIsActive(Candidate.Object), Candidate.DistanceSquared <= FMath::Square(double(Candidate.Definition->LightRadius)));
	}
	for (auto Iterator = Actors.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Wanted.Contains(Iterator.Key()))
		{
			if (AVoxelWorldObjectPresenter* Actor = Iterator.Value().Get())
			{
				Actor->Destroy();
			}
			Iterator.RemoveCurrent();
		}
	}
}
