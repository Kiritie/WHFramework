// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleStatics.h"

#include "Asset/AssetModuleStatics.h"
#include "Containers/Queue.h"
#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

namespace
{
	template<typename TFindSnapshot, typename TForEachSnapshot>
	TMap<FIndex, FVoxelLiquidUpdate> CalculateVoxelLiquidUpdatesInternal(const TFindSnapshot& InFindSnapshot, const TForEachSnapshot& InForEachSnapshot, const TSet<FIndex>& InProtectedIndices)
	{
		TMap<FIndex, FVoxelLiquidUpdate> Updates;
		TMap<FIndex, FVoxelLiquidState> DesiredStates;
		TQueue<FIndex> PendingIndices;
		auto IsSource = [](const FVoxelLiquidSnapshot* InSnapshot)
		{
			return InSnapshot && InSnapshot->bGenerated && InSnapshot->VoxelType == EVoxelType::Water && FVoxelLiquidState(InSnapshot->Data).IsSource();
		};
		auto HasSourceNeighbor = [&IsSource, &InFindSnapshot](FIndex InIndex)
		{
			for(const EDirectionN Direction : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left })
			{
				if(IsSource(InFindSnapshot(InIndex + FMathHelper::DirectionToIndex(Direction)))) return true;
			}
			return false;
		};
		auto IsOpen = [](const FVoxelLiquidSnapshot* InSnapshot)
		{
			return InSnapshot && InSnapshot->bGenerated &&
				(InSnapshot->VoxelType == EVoxelType::Empty || InSnapshot->bCanFlowThrough);
		};
		auto CanFlowInto = [&IsOpen](const FVoxelLiquidSnapshot* InSnapshot)
		{
			return IsOpen(InSnapshot) || InSnapshot && InSnapshot->bGenerated &&
				InSnapshot->VoxelType == EVoxelType::Water && !FVoxelLiquidState(InSnapshot->Data).IsSource();
		};
		auto GetOpenDepth = [&InFindSnapshot, &CanFlowInto](FIndex InIndex) -> int32
		{
			int32 Depth = 0;
			for(int32 Z = 1; Z <= 2; ++Z)
			{
				const FVoxelLiquidSnapshot* Below = InFindSnapshot(InIndex + FIndex(0, 0, -Z));
				if(!Below || !Below->bGenerated) return INDEX_NONE;
				if(!CanFlowInto(Below)) break;
				++Depth;
			}
			return Depth;
		};
		auto AddDesiredState = [&InFindSnapshot, &DesiredStates, &PendingIndices](FIndex InIndex, const FVoxelLiquidState& InState)
		{
			const FVoxelLiquidSnapshot* Snapshot = InFindSnapshot(InIndex);
			if(!Snapshot || !Snapshot->bGenerated) return;
			if(Snapshot->VoxelType == EVoxelType::Water)
			{
				const FVoxelLiquidState SnapshotState(Snapshot->Data);
				if(SnapshotState.IsSource())
				{
					if(!DesiredStates.Contains(InIndex))
					{
						DesiredStates.Add(InIndex, SnapshotState);
						PendingIndices.Enqueue(InIndex);
					}
					return;
				}
			}
			else if(Snapshot->VoxelType != EVoxelType::Empty && !Snapshot->bCanFlowThrough) return;

			FVoxelLiquidState* State = DesiredStates.Find(InIndex);
			if(State)
			{
				if(State->IsSource()) return;
				if(State->IsFalling() && !InState.IsFalling()) return;
				if(State->IsFalling() == InState.IsFalling() && State->GetLevel() <= InState.GetLevel()) return;
			}
			DesiredStates.Add(InIndex, InState);
			PendingIndices.Enqueue(InIndex);
		};

		InForEachSnapshot([&](const FIndex& InIndex, const FVoxelLiquidSnapshot& InSnapshot)
		{
			if(InSnapshot.bGenerated && InSnapshot.VoxelType == EVoxelType::Water)
			{
				const FVoxelLiquidState State(InSnapshot.Data);
				if(State.IsSource())
				{
					for(const EDirectionN Direction : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left, EDirectionN::Down })
					{
						const FIndex NeighborIndex = InIndex + FMathHelper::DirectionToIndex(Direction);
						const FVoxelLiquidSnapshot* Neighbor = InFindSnapshot(NeighborIndex);
						if(CanFlowInto(Neighbor))
						{
							AddDesiredState(InIndex, State);
							break;
						}
					}
				}
			}
		});

		FIndex Index;
		while(PendingIndices.Dequeue(Index))
		{
			const FVoxelLiquidState State = *DesiredStates.Find(Index);
			const int32 OpenDepth = GetOpenDepth(Index);
			if(OpenDepth == INDEX_NONE) continue;
			const FIndex BelowIndex = Index + FIndex(0, 0, -1);
			if(State.IsFalling() && OpenDepth == 0 && HasSourceNeighbor(Index))
			{
				DesiredStates[Index] = FVoxelLiquidState(1);
				PendingIndices.Enqueue(Index);
				continue;
			}
			if(State.IsFalling() && IsSource(InFindSnapshot(BelowIndex))) continue;
			if(State.IsFalling())
			{
				if(OpenDepth > 0)
				{
					AddDesiredState(BelowIndex, FVoxelLiquidState(0, true));
					continue;
				}
			}
			else if(OpenDepth > 0)
			{
				if(State.IsSource())
				{
					AddDesiredState(BelowIndex, FVoxelLiquidState(0, true));
				}
				else if(OpenDepth == 1 && State.GetLevel() < FVoxelLiquidState::MaxLevel)
				{
					AddDesiredState(BelowIndex, FVoxelLiquidState(State.GetLevel() + 1));
				}
				continue;
			}

			const uint8 NextLevel = State.GetLevel() + 1;
			if(NextLevel <= FVoxelLiquidState::MaxLevel)
			{
				for(const EDirectionN Direction : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left })
				{
					const FIndex NeighborIndex = Index + FMathHelper::DirectionToIndex(Direction);
					const FVoxelLiquidSnapshot* Neighbor = InFindSnapshot(NeighborIndex);
					if(!CanFlowInto(Neighbor)) continue;
					const int32 NeighborOpenDepth = GetOpenDepth(NeighborIndex);
					if(NeighborOpenDepth == INDEX_NONE) continue;
					if(NeighborOpenDepth == 1)
					{
						const FIndex DropIndex = NeighborIndex + FIndex(0, 0, -1);
						AddDesiredState(DropIndex, FVoxelLiquidState(NextLevel));
					}
					else if(NeighborOpenDepth > 1)
					{
						if(State.IsSource()) AddDesiredState(NeighborIndex, FVoxelLiquidState(0, true));
					}
					else
					{
						AddDesiredState(NeighborIndex, FVoxelLiquidState(NextLevel));
					}
				}
			}
		}

		InForEachSnapshot([&](const FIndex& InIndex, const FVoxelLiquidSnapshot& InSnapshot)
		{
			if(!InSnapshot.bGenerated) return;
			const bool bWater = InSnapshot.VoxelType == EVoxelType::Water;
			if(bWater && FVoxelLiquidState(InSnapshot.Data).IsSource()) return;
			if(const FVoxelLiquidState* State = DesiredStates.Find(InIndex))
			{
				const FString Data = State->ToData();
				if(!bWater || InSnapshot.Data != Data)
				{
					FVoxelLiquidUpdate Update;
					Update.Data = Data;
					Updates.Add(InIndex, MoveTemp(Update));
				}
			}
			else if(bWater && !InProtectedIndices.Contains(InIndex))
			{
				FVoxelLiquidUpdate Update;
				Update.bRemove = true;
				Updates.Add(InIndex, MoveTemp(Update));
			}
		});
		return Updates;
	}
}

FPrimaryAssetId UVoxelModuleStatics::VoxelTypeToAssetID(EVoxelType InVoxelType)
{
	return UVoxelModule::Get().VoxelTypeToAssetID(InVoxelType);
}

EVoxelTransparency UVoxelModuleStatics::VoxelNatureToTransparency(EVoxelNature InVoxelNature)
{
	switch (InVoxelNature)
	{
		case EVoxelNature::Solid:
		{
			return EVoxelTransparency::Solid;
		}
		case EVoxelNature::SemiSolid:
		case EVoxelNature::TransSolid:
		case EVoxelNature::Foliage:
		{
			return EVoxelTransparency::Semi;
		}
		case EVoxelNature::SmallSemiSolid:
		case EVoxelNature::Liquid:
		case EVoxelNature::SemiLiquid:
		case EVoxelNature::SemiFoliage:
		{
			return EVoxelTransparency::Trans;
		}
		default: break;
	}
	return EVoxelTransparency::Solid;
}

TMap<FIndex, FVoxelLiquidUpdate> UVoxelModuleStatics::CalculateVoxelLiquidUpdates(const TMap<FIndex, FVoxelLiquidSnapshot>& InSnapshots, const TSet<FIndex>& InProtectedIndices)
{
	return CalculateVoxelLiquidUpdatesInternal(
		[&InSnapshots](FIndex InIndex) { return InSnapshots.Find(InIndex); },
		[&InSnapshots](const auto& InFunc)
		{
			for(const auto& Iter : InSnapshots) InFunc(Iter.Key, Iter.Value);
		},
		InProtectedIndices);
}

TMap<FIndex, FVoxelLiquidUpdate> UVoxelModuleStatics::CalculateVoxelLiquidUpdates(const FVoxelLiquidSnapshotGrid& InSnapshots, const TSet<FIndex>& InProtectedIndices)
{
	return CalculateVoxelLiquidUpdatesInternal(
		[&InSnapshots](FIndex InIndex) { return InSnapshots.Find(InIndex); },
		[&InSnapshots](const auto& InFunc)
		{
			for(int32 i = 0; i < InSnapshots.Snapshots.Num(); ++i) InFunc(InSnapshots.GetIndex(i), InSnapshots.Snapshots[i]);
		},
		InProtectedIndices);
}

bool UVoxelModuleStatics::CalculateVoxelLiquidUpdate(FIndex InIndex, const TMap<FIndex, FVoxelLiquidSnapshot>& InSnapshots, FVoxelLiquidUpdate& OutUpdate)
{
	const FVoxelLiquidSnapshot* Current = InSnapshots.Find(InIndex);
	if(!Current || !Current->bGenerated) return false;
	const bool bWater = Current->VoxelType == EVoxelType::Water;
	if(bWater && FVoxelLiquidState(Current->Data).IsSource()) return false;
	if(!bWater && Current->VoxelType != EVoxelType::Empty && !Current->bCanFlowThrough) return false;

	bool bHasState = false;
	FVoxelLiquidState DesiredState;
	auto IsSource = [](const FVoxelLiquidSnapshot* InSnapshot)
	{
		return InSnapshot && InSnapshot->bGenerated && InSnapshot->VoxelType == EVoxelType::Water && FVoxelLiquidState(InSnapshot->Data).IsSource();
	};
	auto HasSourceNeighbor = [&IsSource, &InSnapshots](FIndex InIndex)
	{
		for(const EDirectionN Direction : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left })
		{
			if(IsSource(InSnapshots.Find(InIndex + FMathHelper::DirectionToIndex(Direction)))) return true;
		}
		return false;
	};
	auto IsOpen = [](const FVoxelLiquidSnapshot* InSnapshot)
	{
		return InSnapshot && InSnapshot->bGenerated &&
			(InSnapshot->VoxelType == EVoxelType::Empty || InSnapshot->bCanFlowThrough ||
				InSnapshot->VoxelType == EVoxelType::Water && !FVoxelLiquidState(InSnapshot->Data).IsSource());
	};
	auto GetOpenDepth = [&InSnapshots, &IsOpen](FIndex InVoxelIndex) -> int32
	{
		int32 Depth = 0;
		for(int32 Z = 1; Z <= 2; ++Z)
		{
			const FVoxelLiquidSnapshot* Below = InSnapshots.Find(InVoxelIndex + FIndex(0, 0, -Z));
			if(!Below || !Below->bGenerated) return INDEX_NONE;
			if(!IsOpen(Below)) break;
			++Depth;
		}
		return Depth;
	};

	const FIndex AboveIndex = InIndex + FIndex(0, 0, 1);
	const int32 OpenDepth = GetOpenDepth(InIndex);
	const bool bSourceBeside = HasSourceNeighbor(InIndex);
	const FVoxelLiquidSnapshot* Above = InSnapshots.Find(AboveIndex);
	if(Above && Above->bGenerated && Above->VoxelType == EVoxelType::Water)
	{
		const FVoxelLiquidState AboveState(Above->Data);
		if(AboveState.IsSource() || AboveState.IsFalling() && (!bSourceBeside || OpenDepth > 0))
		{
			DesiredState = FVoxelLiquidState(0, true);
			bHasState = true;
		}
	}
	if(!bHasState && OpenDepth > 1)
	{
		for(const EDirectionN Direction : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left })
		{
			const FIndex NeighborIndex = InIndex + FMathHelper::DirectionToIndex(Direction);
			const FVoxelLiquidSnapshot* Neighbor = InSnapshots.Find(NeighborIndex);
			if(!Neighbor || !Neighbor->bGenerated || Neighbor->VoxelType != EVoxelType::Water ||
				!FVoxelLiquidState(Neighbor->Data).IsSource() || GetOpenDepth(NeighborIndex) != 0) continue;
			DesiredState = FVoxelLiquidState(0, true);
			bHasState = true;
			break;
		}
	}
	if(!bHasState && Above && Above->bGenerated && Above->VoxelType == EVoxelType::Water)
	{
		const FVoxelLiquidState AboveState(Above->Data);
		if(!AboveState.IsSource() && !AboveState.IsFalling() && GetOpenDepth(AboveIndex) == 1 && AboveState.GetLevel() < FVoxelLiquidState::MaxLevel)
		{
			DesiredState = FVoxelLiquidState(AboveState.GetLevel() + 1);
			bHasState = true;
		}
	}
	if(!bHasState)
	{
		uint8 Level = FVoxelLiquidState::MaxLevel + 1;
		const FIndex DropTopIndex = InIndex + FIndex(0, 0, 1);
		const int32 DropDepth = GetOpenDepth(DropTopIndex);
		if(DropDepth == 1 && IsOpen(InSnapshots.Find(DropTopIndex)))
		{
			for(const EDirectionN Direction : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left })
			{
				const FIndex UpperNeighborIndex = DropTopIndex + FMathHelper::DirectionToIndex(Direction);
				const FVoxelLiquidSnapshot* UpperNeighbor = InSnapshots.Find(UpperNeighborIndex);
				if(!UpperNeighbor || !UpperNeighbor->bGenerated || UpperNeighbor->VoxelType != EVoxelType::Water ||
					GetOpenDepth(UpperNeighborIndex) != 0) continue;
				const FVoxelLiquidState UpperNeighborState(UpperNeighbor->Data);
				Level = FMath::Min<uint8>(Level, UpperNeighborState.GetLevel() + 1);
			}
		}
		if(!bHasState && Level <= FVoxelLiquidState::MaxLevel)
		{
			DesiredState = FVoxelLiquidState(Level);
			bHasState = true;
		}
	}
	if(!bHasState)
	{
		if(OpenDepth == 0)
		{
			uint8 Level = FVoxelLiquidState::MaxLevel + 1;
			for(const EDirectionN Direction : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left })
			{
				const FIndex NeighborIndex = InIndex + FMathHelper::DirectionToIndex(Direction);
				const FVoxelLiquidSnapshot* Neighbor = InSnapshots.Find(NeighborIndex);
				if(!Neighbor || !Neighbor->bGenerated || Neighbor->VoxelType != EVoxelType::Water || GetOpenDepth(NeighborIndex) != 0) continue;
				Level = FMath::Min<uint8>(Level, FVoxelLiquidState(Neighbor->Data).GetLevel() + 1);
			}
			if(Level <= FVoxelLiquidState::MaxLevel)
			{
				DesiredState = FVoxelLiquidState(Level);
				bHasState = true;
			}
		}
	}

	if(bHasState)
	{
		const FString Data = DesiredState.ToData();
		if(!bWater || Current->Data != Data)
		{
			OutUpdate.Data = Data;
			return true;
		}
	}
	else if(bWater)
	{
		OutUpdate.bRemove = true;
		return true;
	}
	return false;
}

FIndex UVoxelModuleStatics::LocationToChunkIndex(FVector InLocation)
{
	return UVoxelModule::Get().LocationToChunkIndex(InLocation);
}

FVector UVoxelModuleStatics::ChunkIndexToLocation(FIndex InIndex)
{
	return UVoxelModule::Get().ChunkIndexToLocation(InIndex);
}

FIndex UVoxelModuleStatics::LocationToVoxelIndex(FVector InLocation)
{
	return UVoxelModule::Get().LocationToVoxelIndex(InLocation);
}

FVector UVoxelModuleStatics::VoxelIndexToLocation(FIndex InIndex)
{
	return UVoxelModule::Get().VoxelIndexToLocation(InIndex);
}

int64 UVoxelModuleStatics::VoxelIndexToNumber(FIndex InIndex, bool bWorldSpace)
{
	return UVoxelModule::Get().VoxelIndexToNumber(InIndex, bWorldSpace);
}

FIndex UVoxelModuleStatics::NumberToVoxelIndex(int64 InNumber, bool bWorldSpace)
{
	return UVoxelModule::Get().NumberToVoxelIndex(InNumber, bWorldSpace);
}

FIndex UVoxelModuleStatics::RightAngleToVoxelIndex(ERightAngle InAngle)
{
	switch(InAngle)
	{
		case ERightAngle::RA_0:		return FIndex(0, 0, 0);
		case ERightAngle::RA_90:	return FIndex(-1, 0, 0);
		case ERightAngle::RA_180:	return FIndex(-1, -1, 0);
		case ERightAngle::RA_270:	return FIndex(0, -1, 0);
		default:					return FIndex::ZeroIndex;
	}
}

float UVoxelModuleStatics::GetVoxelNoise1D(float InValue, bool bAbs, bool bUnsigned)
{
	return UVoxelModule::Get().GetVoxelNoise1D(InValue, bAbs, bUnsigned);
}

float UVoxelModuleStatics::GetVoxelNoise2D(FVector2D InLocation, bool bAbs, bool bUnsigned)
{
	return UVoxelModule::Get().GetVoxelNoise2D(InLocation, bAbs, bUnsigned);
}

float UVoxelModuleStatics::GetVoxelNoise3D(FVector InLocation, bool bAbs, bool bUnsigned)
{
	return UVoxelModule::Get().GetVoxelNoise3D(InLocation, bAbs, bUnsigned);
}

FVoxelWorldSaveData& UVoxelModuleStatics::GetVoxelWorldData()
{
	return UVoxelModule::Get().GetWorldData();
}

EVoxelWorldMode UVoxelModuleStatics::GetVoxelWorldMode()
{
	return UVoxelModule::Get().GetWorldMode();
}

void UVoxelModuleStatics::SetVoxelWorldMode(EVoxelWorldMode InWorldMode)
{
	UVoxelModule::Get().SetWorldMode(InWorldMode);
}

EVoxelWorldState UVoxelModuleStatics::GetVoxelWorldState()
{
	return UVoxelModule::Get().GetWorldState();
}

FVoxelWorldBasicSaveData UVoxelModuleStatics::GetVoxelWorldBasicData()
{
	return UVoxelModule::Get().GetWorldBasicData();
}

void UVoxelModuleStatics::LoadVoxelPrefabData(const FVoxelPrefabSaveData& InPrefabData)
{
	return UVoxelModule::Get().LoadPrefabData(InPrefabData);
}

FVoxelPrefabSaveData UVoxelModuleStatics::GetVoxelPrefabData()
{
	return UVoxelModule::Get().GetPrefabData();
}

const FVoxelTopography& UVoxelModuleStatics::GetTopographyByIndex(FIndex InIndex)
{
	return UVoxelModule::Get().GetTopographyByIndex(InIndex);
}

const FVoxelTopography& UVoxelModuleStatics::GetTopographyByLocation(FVector InLocation)
{
	return UVoxelModule::Get().GetTopographyByLocation(InLocation);
}

void UVoxelModuleStatics::SetTopographyByIndex(FIndex InIndex, const FVoxelTopography& InTopography)
{
	UVoxelModule::Get().SetTopographyByIndex(InIndex, InTopography);
}

void UVoxelModuleStatics::SetTopographyByLocation(FVector InLocation, const FVoxelTopography& InTopography)
{
	UVoxelModule::Get().SetTopographyByLocation(InLocation, InTopography);
}

float UVoxelModuleStatics::GetWorldGeneratePercent()
{
	return UVoxelModule::Get().GetWorldGeneratePercent();
}

FBox UVoxelModuleStatics::GetWorldBounds(float InRadius, float InHalfHeight)
{
	return UVoxelModule::Get().GetWorldBounds(InRadius, InHalfHeight);
}

int32 UVoxelModuleStatics::GetChunkNum(bool bNeedGenerated)
{
	return UVoxelModule::Get().GetChunkNum(bNeedGenerated);
}

bool UVoxelModuleStatics::IsChunkGenerated(FIndex InIndex)
{
	return UVoxelModule::Get().IsChunkGenerated(InIndex);
}

FVoxelChunkQueues UVoxelModuleStatics::GetChunkQueues(EVoxelWorldState InWorldState)
{
	return UVoxelModule::Get().GetChunkQueues(InWorldState);
}

UVoxelGenerator* UVoxelModuleStatics::GetVoxelGenerator(const TSubclassOf<UVoxelGenerator>& InClass)
{
	return UVoxelModule::Get().GetVoxelGenerator(InClass);
}

UVoxelChunk* UVoxelModuleStatics::GetChunkByIndex(FIndex InIndex)
{
	return UVoxelModule::Get().GetChunkByIndex(InIndex);
}

UVoxelChunk* UVoxelModuleStatics::GetChunkByLocation(FVector InLocation)
{
	return UVoxelModule::Get().GetChunkByLocation(InLocation);
}

FVoxelItem& UVoxelModuleStatics::GetVoxelByIndex(FIndex InIndex)
{
	return UVoxelModule::Get().GetVoxelByIndex(InIndex);
}

FVoxelItem& UVoxelModuleStatics::GetVoxelByLocation(FVector InLocation)
{
	return UVoxelModule::Get().GetVoxelByLocation(InLocation);
}

UVoxel& UVoxelModuleStatics::GetVoxel(EVoxelType InVoxelType)
{
	return GetVoxel(VoxelTypeToAssetID(InVoxelType));
}

UVoxel& UVoxelModuleStatics::GetVoxel(const FPrimaryAssetId& InVoxelID)
{
	const UVoxelData& VoxelData = UAssetModuleStatics::LoadPrimaryAssetRef<UVoxelData>(InVoxelID);
	if(VoxelData.IsValid())
	{
		const TSubclassOf<UVoxel> VoxelClass = VoxelData.VoxelClass.Get() ? VoxelData.VoxelClass : UVoxel::StaticClass();
		UVoxel& Voxel = UReferencePoolModuleStatics::GetReference<UVoxel>(VoxelClass);
		UReferencePoolModuleStatics::ResetReference<UVoxel>(VoxelClass);
		Voxel.SetItem(InVoxelID);
		return Voxel;
	}
	UVoxel& Voxel = UReferencePoolModuleStatics::GetReference<UVoxel>();
	UReferencePoolModuleStatics::ResetReference<UVoxel>();
	return Voxel;
}

UVoxel& UVoxelModuleStatics::GetVoxel(const FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsEmpty()) return UVoxel::GetEmpty();
	if(InVoxelItem.IsUnknown()) return UVoxel::GetUnknown();
	UVoxel& Voxel = GetVoxel(InVoxelItem.ID);
	Voxel.SetItem(InVoxelItem);
	if(!InVoxelItem.Data.IsEmpty())
	{
		Voxel.LoadData(InVoxelItem.Data);
	}
	return Voxel;
}

bool UVoxelModuleStatics::VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	return UVoxelModule::Get().VoxelRaycastSinge(InRayStart, InRayEnd, InIgnoreActors, OutHitResult);
}

bool UVoxelModuleStatics::VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	return UVoxelModule::Get().VoxelRaycastSinge(InRaycastType, InDistance, InIgnoreActors, OutHitResult);
}

bool UVoxelModuleStatics::VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	return UVoxelModule::Get().VoxelItemTraceSingle(InVoxelItem, InIgnoreActors, OutHitResult);
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter, bool bForce)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InChunkIndex, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter, bForce);
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter, bool bForce)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InLocation, InRange, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter, bForce);
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bCheckVoxel)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InRayStart, InRayEnd, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bCheckVoxel);
}
