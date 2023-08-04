// Fill out your copyright notice in the Description page of Project Settings.PickUp/


#include "Voxel/Chunks/VoxelChunk.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Character/Base/CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Global/GlobalTypes.h"
#include "Math/MathBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Voxel/VoxelModuleBPLibrary.h"

// Sets default values
AVoxelChunk::AVoxelChunk()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SolidMesh = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("SolidMesh"));
	SolidMesh->SetupAttachment(RootComponent);
	SolidMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::Solid);
	SolidMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);

	SemiMesh = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("SemiMesh"));
	SemiMesh->SetupAttachment(RootComponent);
	SemiMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::SemiTransparent);
	SemiMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);

	TransMesh = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("TransMesh"));
	TransMesh->SetupAttachment(RootComponent);
	TransMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::Transparent);
	TransMesh->OnComponentBeginOverlap.AddDynamic(this, &AVoxelChunk::OnBeginOverlap);
	TransMesh->OnComponentEndOverlap.AddDynamic(this, &AVoxelChunk::OnEndOverlap);

	State = EVoxelChunkState::None;
	Batch = -1;
	Index = FIndex::ZeroIndex;
	VoxelMap = TMap<FIndex, FVoxelItem>();
	Neighbors = TMap<EDirection, AVoxelChunk*>();
	ITER_DIRECTION(Iter, Neighbors.Add(Iter); )
	PickUps = TArray<AAbilityPickUpBase*>();
}

void AVoxelChunk::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	if(State != EVoxelChunkState::Spawned) return;
	
	State = EVoxelChunkState::Builded;
	
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();
	for(auto& Iter : SaveData.VoxelDatas)
	{
		const FVoxelItem VoxelItem = FVoxelItem(Iter);
		SetVoxelSample(VoxelItem.Index, VoxelItem);
	}
}

FSaveData* AVoxelChunk::ToData(bool bRefresh)
{
	static FVoxelChunkSaveData SaveData;
	SaveData = FVoxelChunkSaveData();

	SaveData.Index = Index;

	for(auto& Iter : VoxelMap)
	{
		SaveData.VoxelDatas.Add(Iter.Value.ToSaveData(true));
	}

	for(auto& Iter : PickUps)
	{
		SaveData.PickUpDatas.Add(Iter->GetSaveDataRef<FPickUpSaveData>(true));
	}

	return &SaveData;
}

void AVoxelChunk::OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherActor && OtherActor->IsA<ACharacterBase>())
	{
		if(IVoxelAgentInterface* VoxelAgent = Cast<IVoxelAgentInterface>(OtherActor))
		{
			const FVoxelItem& VoxelItem = GetVoxelItem(LocationToIndex(Hit.ImpactPoint - AVoxelModule::Get()->GetWorldData().GetBlockSizedNormal(Hit.ImpactNormal)), true);
			if(VoxelItem.IsValid())
			{
				VoxelItem.GetVoxel().OnAgentHit(VoxelAgent, FVoxelHitResult(VoxelItem, Hit.ImpactPoint, Hit.ImpactNormal));
			}
		}
	}
}

void AVoxelChunk::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
}

void AVoxelChunk::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
}

void AVoxelChunk::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	State = EVoxelChunkState::Spawned;
}

void AVoxelChunk::OnDespawn_Implementation(bool bRecovery)
{
	State = EVoxelChunkState::None;

	AVoxelModule::Get()->GetWorldData().SetChunkData(Index, GetSaveData<FVoxelChunkSaveData>(true));

	for(auto& Iter : VoxelMap)
	{
		DestroyAuxiliary(Iter.Key);
	}
	
	DestroyActors();

	BreakNeighbors();

	Index = FIndex::ZeroIndex;
	Batch = -1;

	SolidMesh->ClearData();
	SemiMesh->ClearData();
	TransMesh->ClearData();

	VoxelMap.Empty();

	SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

	Execute_SetActorVisible(this, false);
}

void AVoxelChunk::SetActorVisible_Implementation(bool bNewVisible)
{
	bVisible = bNewVisible;
	GetRootComponent()->SetVisibility(bNewVisible, true);

	SolidMesh->SetCollisionEnabled(bNewVisible);
	SemiMesh->SetCollisionEnabled(bNewVisible);
	TransMesh->SetCollisionEnabled(bNewVisible);
}

void AVoxelChunk::Initialize(FIndex InIndex, int32 InBatch)
{
	Index = InIndex;
	Batch = InBatch;
	UpdateNeighbors();
}

void AVoxelChunk::Generate(EPhase InPhase)
{
	if(!IsBuilded()) return;

	if(!Execute_IsVisible(this))
	{
		Execute_SetActorVisible(this, true);
	}
	if(PHASEC(InPhase, EPhase::Primary))
	{
		CreateMesh();
		for(auto& Iter : VoxelMap)
		{
			Iter.Value.OnGenerate();
		}
		GetWorldTimerManager().SetTimerForNextTick([this]()
		{
			SpawnActors();
		});
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(IsGenerated())
		{
			BuildMesh();
		}
		CreateMesh();
		if(!IsGenerated())
		{
			for(auto& Iter : VoxelMap)
			{
				Iter.Value.OnGenerate();
			}
		}
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		SpawnActors();
	}
	
	if(State == EVoxelChunkState::Builded)
	{
		State = EVoxelChunkState::Generated;
	}
}

void AVoxelChunk::CreateMesh()
{
	SolidMesh->CreateMesh();
	SemiMesh->CreateMesh();
	TransMesh->CreateMesh();
}

void AVoxelChunk::BuildMap(int32 InStage)
{
	if(State != EVoxelChunkState::Spawned) return;
	
	const auto& WorldData = AVoxelModule::Get()->GetWorldData();
	switch (InStage)
	{
		case 0:
		{
			ITER_INDEX(VoxelIndex, WorldData.ChunkSize, false,
				switch(const EVoxelType VoxelType = UVoxelModuleBPLibrary::GetNoiseVoxelType(LocalIndexToWorld(VoxelIndex)))
				{
					// Tree
					case EVoxelType::Oak:
					case EVoxelType::Birch:
					{
						if(!CheckVoxelNeighbors(VoxelIndex, EVoxelType::Oak, FVector(6.f), true) && !CheckVoxelNeighbors(VoxelIndex, EVoxelType::Birch, FVector(6.f), true))
						{
							SetVoxelSample(VoxelIndex, VoxelType);
						}
						break;
					}
					default:
					{
						SetVoxelSample(VoxelIndex, VoxelType);
						break;
					}
					case EVoxelType::Empty: break;
				}
			)
			break;
		}
		case 1:
		{
			State = EVoxelChunkState::Builded;
			
			const FRandomStream& RandomStream = WorldData.RandomStream;
			for(auto Iter : TMap(VoxelMap))
			{
				const FIndex& VoxelIndex = Iter.Key;
				switch(const EVoxelType& VoxelType = Iter.Value.GetVoxelType())
				{
					// Tree
					case EVoxelType::Oak:
					case EVoxelType::Birch:
					{
						if(!HasVoxel(VoxelIndex + FIndex(0, 0, 1)))
						{
							const EVoxelType LeavesType = (VoxelType == EVoxelType::Oak ? EVoxelType::Oak_Leaves : EVoxelType::Birch_Leaves);
							const int32 TreeHeight = RandomStream.RandRange(4, 5);
							const int32 LeavesHeight = 2;
							const int32 LeavesWidth = RandomStream.FRand() < 0.5f ? 3 : 5;
							DON_WITHINDEX(TreeHeight - 1, i,
								SetVoxelComplex(VoxelIndex + FIndex(0, 0, i + 1), VoxelType);
							)
							for(int32 OffsetZ = TreeHeight - LeavesHeight / 2; OffsetZ <= TreeHeight + LeavesHeight / 2; OffsetZ++)
							{
								for(int32 OffsetX = -LeavesWidth / 2; OffsetX <= LeavesWidth / 2; OffsetX++)
								{
									for(int32 OffsetY = -LeavesWidth / 2; OffsetY <= LeavesWidth / 2; OffsetY++)
									{
										SetVoxelComplex(VoxelIndex + FIndex(OffsetX, OffsetY, OffsetZ - 1), LeavesType);
									}
								}
							}
						}
						break;
					}
					default: break;
				}
			}
			break;
		}
		default: break;
	}
}

void AVoxelChunk::BuildMesh()
{
	for(auto Iter : VoxelMap)
	{
		const FVoxelItem& VoxelItem = Iter.Value;
		if(VoxelItem.IsValid())
		{
			switch(VoxelItem.GetVoxelData().Transparency)
			{
				case EVoxelTransparency::Solid:
				{
					SolidMesh->BuildVoxel(VoxelItem);
					break;
				}
				case EVoxelTransparency::SemiTransparent:
				{
					SemiMesh->BuildVoxel(VoxelItem);
					break;
				}
				case EVoxelTransparency::Transparent:
				{
					TransMesh->BuildVoxel(VoxelItem);
					break;
				}
				default: break;
			}
		}
	}
}


void AVoxelChunk::SpawnActors()
{
	if(State == EVoxelChunkState::Finally) return;

	auto& WorldData = AVoxelModule::Get()->GetWorldData();
	if(WorldData.IsExistChunkData(Index))
	{
		State = EVoxelChunkState::Finally;
		LoadActors(WorldData.GetChunkData(Index));
	}
	else
	{
		for(auto Iter : AVoxelModule::Get()->GetVerticalChunks(Index))
		{
			Iter->State = EVoxelChunkState::Finally;
		}
		GenerateActors();
	}
}

void AVoxelChunk::LoadActors(FSaveData* InSaveData)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();
	for(auto Iter : SaveData.PickUpDatas)
	{
		UAbilityModuleBPLibrary::SpawnPickUp(&Iter, this);
	}
}

void AVoxelChunk::GenerateActors()
{
}

void AVoxelChunk::DestroyActors()
{
	UObjectPoolModuleBPLibrary::DespawnObjects(PickUps);
	PickUps.Empty();
}

void AVoxelChunk::GenerateNeighbors(FIndex InIndex, EPhase InPhase)
{
	GenerateNeighbors(InIndex.X, InIndex.Y, InIndex.Z, InPhase);
}

void AVoxelChunk::GenerateNeighbors(int32 InX, int32 InY, int32 InZ, EPhase InPhase)
{
	const auto& WorldData = AVoxelModule::Get()->GetWorldData();
	if(InX <= 0 && GetNeighbor(EDirection::Backward))
	{
		GetNeighbor(EDirection::Backward)->Generate(InPhase);
	}
	else if(InX >= WorldData.ChunkSize.X - 1 && GetNeighbor(EDirection::Forward))
	{
		GetNeighbor(EDirection::Forward)->Generate(InPhase);
	}
	if(InY <= 0 && GetNeighbor(EDirection::Left))
	{
		GetNeighbor(EDirection::Left)->Generate(InPhase);
	}
	else if(InY >= WorldData.ChunkSize.Y - 1 && GetNeighbor(EDirection::Right))
	{
		GetNeighbor(EDirection::Right)->Generate(InPhase);
	}
	if(InZ <= 0 && GetNeighbor(EDirection::Down))
	{
		GetNeighbor(EDirection::Down)->Generate(InPhase);
	}
	else if(InZ >= WorldData.ChunkSize.Z - 1 && GetNeighbor(EDirection::Up))
	{
		GetNeighbor(EDirection::Up)->Generate(InPhase);
	}
}

void AVoxelChunk::UpdateNeighbors()
{
	ITER_DIRECTION(Direction,
		Neighbors[Direction] = AVoxelModule::Get()->FindChunkByIndex(Index + UMathBPLibrary::DirectionToIndex(Direction));
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[UMathBPLibrary::InvertDirection(Direction)] = this;
		}
	)
}

void AVoxelChunk::BreakNeighbors()
{
	ITER_DIRECTION(Direction,
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[UMathBPLibrary::InvertDirection(Direction)] = nullptr;
			Neighbors[Direction] = nullptr;
		}
	)
}

bool AVoxelChunk::IsOnTheChunk(FIndex InIndex) const
{
	const auto& WorldData = AVoxelModule::Get()->GetWorldData();
	return InIndex.X >= 0 && InIndex.X < WorldData.ChunkSize.X &&
		InIndex.Y >= 0 && InIndex.Y < WorldData.ChunkSize.Y &&
		InIndex.Z >= 0 && InIndex.Z < WorldData.ChunkSize.Z;
}

bool AVoxelChunk::IsOnTheChunk(FVector InLocation) const
{
	const auto& WorldData = AVoxelModule::Get()->GetWorldData();
	return InLocation.X >= GetChunkLocation().X && InLocation.X < GetChunkLocation().X + WorldData.GetChunkRealSize().X &&
		InLocation.Y >= GetChunkLocation().Y && InLocation.Y < GetChunkLocation().Y + WorldData.GetChunkRealSize().Y &&
		InLocation.Z >= GetChunkLocation().Z && InLocation.Z < GetChunkLocation().Z + WorldData.GetChunkRealSize().Z;
}

FIndex AVoxelChunk::LocationToIndex(FVector InLocation, bool bWorldSpace /*= true*/) const
{
	InLocation = (InLocation - (bWorldSpace ? GetChunkLocation() : FVector::ZeroVector)) / AVoxelModule::Get()->GetWorldData().BlockSize;
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), FMath::FloorToInt(InLocation.Z));
}

FVector AVoxelChunk::IndexToLocation(FIndex InIndex, bool bWorldSpace /*= true*/) const
{
	return InIndex.ToVector() * AVoxelModule::Get()->GetWorldData().BlockSize + (bWorldSpace ? GetChunkLocation() : FVector::ZeroVector);
}

FIndex AVoxelChunk::LocalIndexToWorld(FIndex InIndex) const
{
	return InIndex + Index * AVoxelModule::Get()->GetWorldData().ChunkSize;
}

FIndex AVoxelChunk::WorldIndexToLocal(FIndex InIndex) const
{
	return InIndex - Index * AVoxelModule::Get()->GetWorldData().ChunkSize;
}

bool AVoxelChunk::HasVoxel(FIndex InIndex)
{
	return HasVoxel(InIndex.X, InIndex.Y, InIndex.Z);
}

bool AVoxelChunk::HasVoxel(int32 InX, int32 InY, int32 InZ)
{
	const auto& WorldData = AVoxelModule::Get()->GetWorldData();
	if(InX < 0)
	{
		if(GetNeighbor(EDirection::Backward))
		{
			return GetNeighbor(EDirection::Backward)->HasVoxel(InX + WorldData.ChunkSize.X, InY, InZ);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(GetNeighbor(EDirection::Forward))
		{
			return GetNeighbor(EDirection::Forward)->HasVoxel(InX - WorldData.ChunkSize.X, InY, InZ);
		}
	}
	else if(InY < 0)
	{
		if(GetNeighbor(EDirection::Left))
		{
			return GetNeighbor(EDirection::Left)->HasVoxel(InX, InY + WorldData.ChunkSize.Y, InZ);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(GetNeighbor(EDirection::Right))
		{
			return GetNeighbor(EDirection::Right)->HasVoxel(InX, InY - WorldData.ChunkSize.Y, InZ);
		}
	}
	else if(InZ < 0)
	{
		if(GetNeighbor(EDirection::Down))
		{
			return GetNeighbor(EDirection::Down)->HasVoxel(InX, InY, InZ + WorldData.ChunkSize.Z);
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(GetNeighbor(EDirection::Up))
		{
			return GetNeighbor(EDirection::Up)->HasVoxel(InX, InY, InZ - WorldData.ChunkSize.Z);
		}
	}
	return VoxelMap.Contains(FIndex(InX, InY, InZ));
}

UVoxel& AVoxelChunk::GetVoxel(FIndex InIndex, bool bMainPart)
{
	return GetVoxel(InIndex.X, InIndex.Y, InIndex.Z, bMainPart);
}

UVoxel& AVoxelChunk::GetVoxel(int32 InX, int32 InY, int32 InZ, bool bMainPart)
{
	return GetVoxelItem(InX, InY, InZ, bMainPart).GetVoxel();
}

FVoxelItem& AVoxelChunk::GetVoxelItem(FIndex InIndex, bool bMainPart)
{
	return GetVoxelItem(InIndex.X, InIndex.Y, InIndex.Z, bMainPart);
}

FVoxelItem& AVoxelChunk::GetVoxelItem(int32 InX, int32 InY, int32 InZ, bool bMainPart)
{
	const auto& WorldData = AVoxelModule::Get()->GetWorldData();
	if(InX < 0)
	{
		if(GetNeighbor(EDirection::Backward))
		{
			return GetNeighbor(EDirection::Backward)->GetVoxelItem(InX + WorldData.ChunkSize.X, InY, InZ, bMainPart);
		}
		return WorldData.WorldSize.X == -1.f ? FVoxelItem::Unknown : FVoxelItem::Empty;
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(GetNeighbor(EDirection::Forward))
		{
			return GetNeighbor(EDirection::Forward)->GetVoxelItem(InX - WorldData.ChunkSize.X, InY, InZ, bMainPart);
		}
		return WorldData.WorldSize.X == -1.f ? FVoxelItem::Unknown : FVoxelItem::Empty;
	}
	else if(InY < 0)
	{
		if(GetNeighbor(EDirection::Left))
		{
			return GetNeighbor(EDirection::Left)->GetVoxelItem(InX, InY + WorldData.ChunkSize.Y, InZ, bMainPart);
		}
		return WorldData.WorldSize.Y == -1.f ? FVoxelItem::Unknown : FVoxelItem::Empty;
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(GetNeighbor(EDirection::Right))
		{
			return GetNeighbor(EDirection::Right)->GetVoxelItem(InX, InY - WorldData.ChunkSize.Y, InZ, bMainPart);
		}
		return WorldData.WorldSize.Y == -1.f ? FVoxelItem::Unknown : FVoxelItem::Empty;
	}
	else if(InZ < 0)
	{
		if(GetNeighbor(EDirection::Down))
		{
			return GetNeighbor(EDirection::Down)->GetVoxelItem(InX, InY, InZ + WorldData.ChunkSize.Z, bMainPart);
		}
		else if(Index.Z > 0)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(GetNeighbor(EDirection::Up))
		{
			return GetNeighbor(EDirection::Up)->GetVoxelItem(InX, InY, InZ - WorldData.ChunkSize.Z, bMainPart);
		}
		else if(Index.Z < WorldData.WorldSize.Z)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(HasVoxel(InX, InY, InZ))
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		if(bMainPart)
		{
			const UVoxelData& VoxelData = VoxelMap[VoxelIndex].GetVoxelData();
			if(VoxelData.PartType != EVoxelPartType::Main)
			{
				return GetVoxelItem(VoxelIndex - VoxelData.PartIndex);
			}
		}
		return VoxelMap[VoxelIndex];
	}
	return FVoxelItem::Empty;
}

bool AVoxelChunk::CheckVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, FVector InRange/* = FVector::OneVector*/)
{
	ITER_INDEX(Iter, InRange, false,
		if(!GetVoxelItem(InIndex + Iter).IsReplaceable(InVoxelItem))
		{
			return true;
		}
	)
	return false;
}

bool AVoxelChunk::CheckVoxelAdjacent(FIndex InIndex, EDirection InDirection)
{
	const FVoxelItem& VoxelItem = GetVoxelItem(InIndex);
	const FIndex AdjacentIndex = UMathBPLibrary::GetAdjacentIndex(InIndex, InDirection, VoxelItem.Angle);
	
	if(!VoxelItem.IsValid() || LocalIndexToWorld(AdjacentIndex).Z <= 0) return true;
	
	const FVoxelItem& AdjacentItem = GetVoxelItem(AdjacentIndex);
	if(AdjacentItem.IsValid())
	{
		const UVoxelData& VoxelData = VoxelItem.GetVoxelData();
		const UVoxelData& AdjacentData = AdjacentItem.GetVoxelData();
		switch(VoxelData.Transparency)
		{
			case EVoxelTransparency::Solid:
			{
				switch(AdjacentData.Transparency)
				{
					case EVoxelTransparency::Solid:
					{
						return true;
					}
					default: break;
				}
				break;
			}
			case EVoxelTransparency::SemiTransparent:
			{
				switch(AdjacentData.Transparency)
				{
					case EVoxelTransparency::SemiTransparent:
					{
						if(VoxelData.VoxelType == AdjacentData.VoxelType)
						{
							switch(VoxelData.VoxelType)
							{
								case EVoxelType::Oak_Leaves:
								case EVoxelType::Birch_Leaves:
								case EVoxelType::Ice:
								case EVoxelType::Glass:
								{
									return true;
								}
								default: break;
							}
						}
						break;
					}
					default: break;
				}
				break;
			}
			case EVoxelTransparency::Transparent:
			{
				switch(AdjacentData.Transparency)
				{
					case EVoxelTransparency::Solid:
					case EVoxelTransparency::SemiTransparent:
					{
						return true;
					}
					case EVoxelTransparency::Transparent:
					{
						if(VoxelData.VoxelType == AdjacentData.VoxelType)
						{
							switch(VoxelData.VoxelType)
							{
								case EVoxelType::Water:
								{
									return true;
								}
								default: break;
							}
						}
						break;
					}
				}
				break;
			}
		}
	}
	else if(AdjacentItem.IsUnknown())
	{
		return true;
	}
	return false;
}

bool AVoxelChunk::CheckVoxelNeighbors(FIndex InIndex, EVoxelType InVoxelType, FVector InRange, bool bFromCenter, bool bIgnoreBottom)
{
	ITER_INDEX(Iter1, InRange, bFromCenter,
		ITER_DIRECTION(Iter2, 
			if(!bIgnoreBottom || Iter2 != EDirection::Down)
			{
				const FIndex NeighborIndex = InIndex + Iter1 + UMathBPLibrary::DirectionToIndex(Iter2);
				if(IsOnTheChunk(NeighborIndex))
				{
					FVoxelItem& NeighborItem = GetVoxelItem(NeighborIndex);
					if(NeighborItem.IsValid() && NeighborItem.GetVoxelData().VoxelType == InVoxelType)
					{
						return true;
					}
				}
			}
		)
	)
	return false;
}

bool AVoxelChunk::SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	bool bSuccess = false;
	
	if(InVoxelItem.IsValid())
	{
		if(IsOnTheChunk(InIndex))
		{
			FVoxelItem VoxelItem = FVoxelItem(InVoxelItem);
			VoxelItem.Owner = this;
			VoxelItem.Index = InIndex;
			VoxelMap.Emplace(InIndex, VoxelItem);
			if(bGenerate) VoxelMap[InIndex].OnGenerate(InAgent);
			bSuccess = true;
		}
	}
	else if(HasVoxel(InIndex))
	{
		if(bGenerate) VoxelMap[InIndex].OnDestroy(InAgent);
		VoxelMap.Remove(InIndex);
		bSuccess = true;
	}

	if(bSuccess && bGenerate)
	{
		Generate(EPhase::Lesser);
		GenerateNeighbors(InIndex, EPhase::Lesser);
	}

	return bSuccess;
}

bool AVoxelChunk::SetVoxelSample(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	return SetVoxelSample(FIndex(InX, InY, InZ), InVoxelItem, bGenerate, InAgent);
}

bool AVoxelChunk::SetVoxelComplex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	return SetVoxelComplex(InIndex.X, InIndex.Y, InIndex.Z, InVoxelItem, bGenerate, InAgent);
}

bool AVoxelChunk::SetVoxelComplex(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	const auto& WorldData = AVoxelModule::Get()->GetWorldData();
	if(InX < 0)
	{
		if(GetNeighbor(EDirection::Backward))
		{
			return GetNeighbor(EDirection::Backward)->SetVoxelComplex(InX + WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(GetNeighbor(EDirection::Forward))
		{
			return GetNeighbor(EDirection::Forward)->SetVoxelComplex(InX - WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY < 0)
	{
		if(GetNeighbor(EDirection::Left))
		{
			return GetNeighbor(EDirection::Left)->SetVoxelComplex(InX, InY + WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(GetNeighbor(EDirection::Right))
		{
			return GetNeighbor(EDirection::Right)->SetVoxelComplex(InX, InY - WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ < 0)
	{
		if(GetNeighbor(EDirection::Down))
		{
			return GetNeighbor(EDirection::Down)->SetVoxelComplex(InX, InY, InZ + WorldData.ChunkSize.Z, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(!GetNeighbor(EDirection::Up))
		{
			AVoxelModule::Get()->SpawnChunk(Index + UMathBPLibrary::DirectionToIndex(EDirection::Up), !bGenerate);
		}
		if(GetNeighbor(EDirection::Up))
		{
			return GetNeighbor(EDirection::Up)->SetVoxelComplex(InX, InY, InZ - WorldData.ChunkSize.Z, InVoxelItem, bGenerate, InAgent);
		}
	}
	else
	{
		const bool bDestroying = !InVoxelItem.IsValid();
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		FVoxelItem VoxelItem = !bDestroying ? InVoxelItem : GetVoxelItem(Index);
		UVoxelData& VoxelData = VoxelItem.GetVoxelData(false);
		if(VoxelData.IsValid() && VoxelData.PartType == EVoxelPartType::Main)
		{
			const FVector Range = VoxelData.GetRange(VoxelItem.Angle);
			if(!CheckVoxel(VoxelIndex, InVoxelItem, Range))
			{
				if(bDestroying)
				{
					// Replace with water
					if(CheckVoxelNeighbors(VoxelIndex, EVoxelType::Water, Range, false, true))
					{
						VoxelItem.ID = UVoxelModuleBPLibrary::VoxelTypeToAssetID(EVoxelType::Water);
					}
					else
					{
						VoxelItem.ID = FPrimaryAssetId();
					}
				}
				bool bSuccess = true;
				ITER_INDEX(PartIndex, Range, false,
					UVoxelData& PartData = VoxelData.GetPartData(PartIndex);
					if(PartData.IsValid())
					{
						if(!bDestroying) VoxelItem.ID = PartData.GetPrimaryAssetId();
						if(PartData.PartType == EVoxelPartType::Main)
						{
							if(!SetVoxelSample(VoxelIndex, VoxelItem, bGenerate, InAgent))
							{
								bSuccess = false;
							}
						}
						else
						{
							if(!SetVoxelComplex(VoxelIndex + PartIndex, VoxelItem, bGenerate, InAgent))
							{
								bSuccess = false;
							}
						}
					}
					else
					{
						bSuccess = false;
					}
				)
				return bSuccess;
			}
		}
		else
		{
			return SetVoxelSample(VoxelIndex, InVoxelItem, bGenerate, InAgent);
		}
	}
	return false;
}

void AVoxelChunk::AddSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>() || ISceneActorInterface::Execute_GetContainer(InActor) == this) return;

	if(State == EVoxelChunkState::None)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(InActor);
	}
	else
	{
		if(ISceneActorInterface::Execute_GetContainer(InActor))
		{
			ISceneActorInterface::Execute_GetContainer(InActor)->RemoveSceneActor(InActor);
		}

		ISceneActorInterface::Execute_SetContainer(InActor, this);

		InActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

		if(AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(InActor))
		{
			PickUps.Add(PickUp);
		}
	}
}

void AVoxelChunk::RemoveSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>() || ISceneActorInterface::Execute_GetContainer(InActor) != this) return;

	ISceneActorInterface::Execute_SetContainer(InActor, nullptr);

	InActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if(AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(InActor))
	{
		PickUps.Remove(PickUp);
	}
}

AVoxelAuxiliary* AVoxelChunk::SpawnAuxiliary(FIndex InIndex)
{
	const auto& VoxelItem = GetVoxelItem(InIndex);
	if(VoxelItem.IsValid() && !VoxelItem.Auxiliary)
	{
		const auto& VoxelData = VoxelItem.GetVoxelData();
		if(VoxelData.AuxiliaryClass && VoxelData.PartType == EVoxelPartType::Main)
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelAuxiliary>(nullptr, VoxelData.AuxiliaryClass))
			{
				AddSceneActor(Auxiliary);
				Auxiliary->Initialize(VoxelItem);
				return Auxiliary;
			}
		}
	}
	return nullptr;
}

void AVoxelChunk::DestroyAuxiliary(FIndex InIndex)
{
	const auto& VoxelItem = GetVoxelItem(InIndex);
	if(VoxelItem.Auxiliary)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(VoxelItem.Auxiliary);
	}
}

FVector AVoxelChunk::GetChunkLocation() const
{
	return UVoxelModuleBPLibrary::ChunkIndexToLocation(Index);
}
