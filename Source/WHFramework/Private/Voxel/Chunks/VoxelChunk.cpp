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
#include "Voxel/Datas/VoxelTreeData.h"
#include "Voxel/Voxels/VoxelTree.h"

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

	Batch = -1;
	Index = FIndex::ZeroIndex;
	bBuilded = false;
	bGenerated = false;
	VoxelMap = TMap<FIndex, FVoxelItem>();
	Neighbors = TMap<EDirection, AVoxelChunk*>();
	ITER_DIRECTION(Iter, Neighbors.Add(Iter); )
	PickUps = TArray<AAbilityPickUpBase*>();
}

void AVoxelChunk::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();
	TArray<FString> VoxelDatas;
	SaveData.VoxelDatas.ParseIntoArray(VoxelDatas, TEXT("/"));
	for(auto& Iter : VoxelDatas)
	{
		const FVoxelItem VoxelItem = FVoxelItem(Iter);
		SetVoxelSample(VoxelItem.Index, VoxelItem);
	}
	bBuilded = true;
}

FSaveData* AVoxelChunk::ToData(bool bRefresh)
{
	static FVoxelChunkSaveData SaveData;
	SaveData = FVoxelChunkSaveData();

	SaveData.Index = Index;
	SaveData.bBuilded = bBuilded;
	SaveData.bGenerated = bGenerated;

	for(auto& Iter : VoxelMap)
	{
		SaveData.VoxelDatas.Appendf(TEXT("/%s"), *Iter.Value.ToSaveData(bRefresh));
	}

	for(auto& Iter : PickUps)
	{
		SaveData.PickUpDatas.Add(Iter->GetSaveDataRef<FPickUpSaveData>(bRefresh));
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
	
}

void AVoxelChunk::OnDespawn_Implementation(bool bRecovery)
{
	AVoxelModule::Get()->GetWorldData().SetChunkData(Index, GetSaveData<FVoxelChunkSaveData>(true));

	for(auto& Iter : VoxelMap)
	{
		DestroyAuxiliary(Iter.Value);
	}
	
	DestroyActors();

	BreakNeighbors();

	Index = FIndex::ZeroIndex;
	Batch = -1;
	bBuilded = false;
	bGenerated = false;

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
		if(!bGenerated)
		{
			bGenerated = true;
			SpawnActors();
		}
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bGenerated)
		{
			BuildMesh();
		}
		CreateMesh();
		for(auto& Iter : VoxelMap)
		{
			Iter.Value.OnGenerate();
		}
		if(!bGenerated)
		{
			bBuilded = true;
			bGenerated = true;
		}
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		if(bGenerated)
		{
			SpawnActors();
		}
	}
}

void AVoxelChunk::UnGenerate(EPhase InPhase)
{
	if(PHASEC(InPhase, EPhase::Final))
	{
		if(bGenerated)
		{
			DestroyActors();
		}
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
	switch (InStage)
	{
		case 0:
		{
			const auto& WorldData = AVoxelModule::Get()->GetWorldData();
			ITER_INDEX(VoxelIndex, WorldData.ChunkSize, false,
				const EVoxelType VoxelType = UVoxelModuleBPLibrary::GetNoiseVoxelType(LocalIndexToWorld(VoxelIndex));
				FVoxelItem VoxelItem = FVoxelItem(VoxelType);
				switch(VoxelType)
				{
					// Tree
					case EVoxelType::Oak:
					case EVoxelType::Birch:
					{
						const auto& TreeData = VoxelItem.GetData<UVoxelTreeData>();
						if(!CheckVoxelNeighbors(VoxelIndex, VoxelType, TreeData.TreeRange, true, false, true))
						{
							auto& VoxelTree = VoxelItem.GetVoxel<UVoxelTree>();
							VoxelTree.InitTree(EVoxelTreePart::Root);
							VoxelItem.Data = VoxelTree.ToData();
							SetVoxelSample(VoxelIndex, VoxelItem);
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
			for(auto Iter : TMap(VoxelMap))
			{
				auto& VoxelData = Iter.Value.GetVoxelData();
				switch(VoxelData.VoxelType)
				{
					// Tree
					case EVoxelType::Oak:
					case EVoxelType::Birch:
					{
						UVoxelTree& VoxelTree = Iter.Value.GetVoxel<UVoxelTree>();
						if(VoxelTree.GetTreePart() == EVoxelTreePart::Root)
						{
							VoxelTree.BuildTree();
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
	bBuilded = true;
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
	auto& WorldData = AVoxelModule::Get()->GetWorldData();
	if(WorldData.IsGenerateChunkData(Index))
	{
		LoadActors(WorldData.GetChunkData(Index));
	}
	else if(Index.Z == WorldData.WorldSize.Z - 1)
	{
		CreateActors();
	}
}

void AVoxelChunk::LoadActors(FSaveData* InSaveData)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();
	for(auto& Iter : SaveData.PickUpDatas)
	{
		UAbilityModuleBPLibrary::SpawnPickUp(&Iter, this);
	}
}

void AVoxelChunk::CreateActors()
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
	if(InX <= 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Backward))
		{
			Chunk->Generate(InPhase);
		}
	}
	else if(InX >= WorldData.ChunkSize.X - 1)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Forward))
		{
			Chunk->Generate(InPhase);
		}
	}
	if(InY <= 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Left))
		{
			Chunk->Generate(InPhase);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y - 1)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Right))
		{
			Chunk->Generate(InPhase);
		}
	}
	if(InZ <= 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Down))
		{
			Chunk->Generate(InPhase);
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z - 1)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Up))
		{
			Chunk->Generate(InPhase);
		}
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

bool AVoxelChunk::LocalIndexToNeighbor(FIndex InIndex, EDirection& OutDirection) const
{
	const auto& WorldData = AVoxelModule::Get()->GetWorldData();
	if(InIndex.X < 0)
	{
		OutDirection = EDirection::Backward;
		return true;
	}
	else if(InIndex.X >= WorldData.ChunkSize.X)
	{
		OutDirection = EDirection::Forward;
		return true;
	}
	else if(InIndex.Y < 0)
	{
		OutDirection = EDirection::Left;
		return true;
	}
	else if(InIndex.Y >= WorldData.ChunkSize.Y)
	{
		OutDirection = EDirection::Right;
		return true;
	}
	else if(InIndex.Z < 0)
	{
		OutDirection = EDirection::Down;
		return true;
	}
	else if(InIndex.Z >= WorldData.ChunkSize.Z)
	{
		OutDirection = EDirection::Up;
		return true;
	}
	return false;
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
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Backward))
		{
			return Chunk->HasVoxel(InX + WorldData.ChunkSize.X, InY, InZ);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Forward))
		{
			return Chunk->HasVoxel(InX - WorldData.ChunkSize.X, InY, InZ);
		}
	}
	else if(InY < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Left))
		{
			return Chunk->HasVoxel(InX, InY + WorldData.ChunkSize.Y, InZ);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Right))
		{
			return Chunk->HasVoxel(InX, InY - WorldData.ChunkSize.Y, InZ);
		}
	}
	else if(InZ < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Down))
		{
			return Chunk->HasVoxel(InX, InY, InZ + WorldData.ChunkSize.Z);
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Up))
		{
			return Chunk->HasVoxel(InX, InY, InZ - WorldData.ChunkSize.Z);
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
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Backward))
		{
			return Chunk->GetVoxelItem(InX + WorldData.ChunkSize.X, InY, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.X == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Forward))
		{
			return Chunk->GetVoxelItem(InX - WorldData.ChunkSize.X, InY, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.X == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InY < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Left))
		{
			return Chunk->GetVoxelItem(InX, InY + WorldData.ChunkSize.Y, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.Y == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Right))
		{
			return Chunk->GetVoxelItem(InX, InY - WorldData.ChunkSize.Y, InZ, bMainPart);
		}
		else if(WorldData.WorldSize.Y == -1.f)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InZ < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Down))
		{
			return Chunk->GetVoxelItem(InX, InY, InZ + WorldData.ChunkSize.Z, bMainPart);
		}
		else if(Index.Z > 0)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Up))
		{
			return Chunk->GetVoxelItem(InX, InY, InZ - WorldData.ChunkSize.Z, bMainPart);
		}
		else if(Index.Z < WorldData.WorldSize.Z)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(HasVoxel(InX, InY, InZ))
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		if(bMainPart) return VoxelMap[VoxelIndex].GetMain();
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
	return CheckVoxelAdjacent(GetVoxelItem(InIndex), InDirection);
}

bool AVoxelChunk::CheckVoxelAdjacent(const FVoxelItem& InVoxelItem, EDirection InDirection)
{
	const FIndex AdjacentIndex = UMathBPLibrary::GetAdjacentIndex(InVoxelItem.Index, InDirection, InVoxelItem.Angle);
	
	if(!InVoxelItem.IsValid() || LocalIndexToWorld(AdjacentIndex).Z <= 0) return true;
	
	const FVoxelItem& AdjacentItem = GetVoxelItem(AdjacentIndex);
	if(AdjacentItem.IsValid())
	{
		const UVoxelData& VoxelData = InVoxelItem.GetVoxelData();
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

bool AVoxelChunk::CheckVoxelNeighbors(FIndex InIndex, EVoxelType InVoxelType, FVector InRange, bool bFromCenter, bool bIgnoreBottom, bool bOnTheChunk)
{
	ITER_INDEX(Iter1, InRange, bFromCenter,
		ITER_DIRECTION(Iter2, 
			if(!bIgnoreBottom || Iter2 != EDirection::Down)
			{
				const FIndex NeighborIndex = InIndex + Iter1 + UMathBPLibrary::DirectionToIndex(Iter2);
				if(!bOnTheChunk || IsOnTheChunk(NeighborIndex))
				{
					FVoxelItem& NeighborItem = GetVoxelItem(NeighborIndex);
					if(NeighborItem.IsValid() && NeighborItem.GetVoxelType() == InVoxelType)
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
			FVoxelItem VoxelItem = InVoxelItem;
			VoxelItem.Owner = this;
			VoxelItem.Index = InIndex;
			VoxelMap.Emplace(InIndex, VoxelItem);
			if(bGenerate) VoxelMap[InIndex].OnGenerate(InAgent);
			bSuccess = true;
		}
	}
	else
	{
		if(HasVoxel(InIndex))
		{
			FVoxelItem VoxelItem = VoxelMap[InIndex];
			VoxelMap.Remove(InIndex);
			if(bGenerate) VoxelItem.OnDestroy(InAgent);
			bSuccess = true;
		}
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
		if(AVoxelChunk* Chunk = GetOrSpawnNeighbor(EDirection::Backward, !bGenerate))
		{
			return Chunk->SetVoxelComplex(InX + WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(AVoxelChunk* Chunk = GetOrSpawnNeighbor(EDirection::Forward, !bGenerate))
		{
			return Chunk->SetVoxelComplex(InX - WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY < 0)
	{
		if(AVoxelChunk* Chunk = GetOrSpawnNeighbor(EDirection::Left, !bGenerate))
		{
			return Chunk->SetVoxelComplex(InX, InY + WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(AVoxelChunk* Chunk = GetOrSpawnNeighbor(EDirection::Right, !bGenerate))
		{
			return Chunk->SetVoxelComplex(InX, InY - WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ < 0)
	{
		if(AVoxelChunk* Chunk = GetOrSpawnNeighbor(EDirection::Down, !bGenerate))
		{
			return Chunk->SetVoxelComplex(InX, InY, InZ + WorldData.ChunkSize.Z, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ >= WorldData.ChunkSize.Z)
	{
		if(AVoxelChunk* Chunk = GetOrSpawnNeighbor(EDirection::Up, !bGenerate))
		{
			return Chunk->SetVoxelComplex(InX, InY, InZ - WorldData.ChunkSize.Z, InVoxelItem, bGenerate, InAgent);
		}
	}
	else
	{
		const FIndex VoxelIndex = FIndex(InX, InY, InZ);
		const FVoxelItem VoxelItem = InVoxelItem.IsValid() ? InVoxelItem : GetVoxelItem(VoxelIndex);
		if(VoxelItem.IsValid())
		{
			UVoxelData& VoxelData = VoxelItem.GetVoxelData(false);
			const FVector VoxelRange = VoxelData.GetRange(VoxelItem.Angle);
			if(!CheckVoxel(VoxelIndex, InVoxelItem, VoxelRange))
			{
				if(VoxelData.bMainPart && VoxelRange != FVector::OneVector)
				{
					TMap<FIndex, FVoxelItem> VoxelItems;
					VoxelItems.Emplace(VoxelIndex, InVoxelItem.IsValid() ? VoxelData.GetPrimaryAssetId() : FPrimaryAssetId());
					ITER_INDEX(PartIndex, VoxelRange, false,
						UVoxelData& PartData = VoxelData.GetPartData(PartIndex);
						if(!PartData.bMainPart)
						{
							VoxelItems.Emplace(VoxelIndex + PartIndex, InVoxelItem.IsValid() ? PartData.GetPrimaryAssetId() : FPrimaryAssetId());
						}
					)
					return SetVoxelComplex(VoxelItems, bGenerate, true, InAgent);
				}
				else
				{
					return SetVoxelSample(VoxelIndex, InVoxelItem, bGenerate, InAgent);
				}
			}
		}
	}
	return false;
}

bool AVoxelChunk::SetVoxelComplex(const TMap<FIndex, FVoxelItem>& InVoxelItems, bool bGenerate, bool bFirstSample, IVoxelAgentInterface* InAgent)
{
	bool bSuccess = true;
	TArray<EDirection> neighbors;
	for(auto& Iter : InVoxelItems)
	{
		FVoxelItem VoxelItem = GetVoxelItem(Iter.Key);
		if(bFirstSample ? !SetVoxelSample(Iter.Key, Iter.Value, false, InAgent) :
			!SetVoxelComplex(Iter.Key, Iter.Value, false, InAgent))
		{
			bSuccess = false;
		}
		else if(bGenerate)
		{
			if(Iter.Value.IsValid())
			{
				GetVoxelItem(Iter.Key).OnGenerate(InAgent);
			}
			else
			{
				VoxelItem.OnDestroy(InAgent);
			}
			EDirection neighbor;
			if(LocalIndexToNeighbor(Iter.Key, neighbor))
			{
				neighbors.AddUnique(neighbor);
			}
		}
		bFirstSample = false;
	}
	if(bGenerate)
	{
		Generate(EPhase::Lesser);
		for(const auto Iter : neighbors)
		{
			if(GetNeighbor(Iter))
			{
				GetNeighbor(Iter)->Generate(EPhase::Lesser);
			}
		}
	}
	return bSuccess;
}

void AVoxelChunk::AddSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>() || ISceneActorInterface::Execute_GetContainer(InActor) == this) return;

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

AVoxelAuxiliary* AVoxelChunk::SpawnAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && !InVoxelItem.Auxiliary)
	{
		const auto& VoxelData = InVoxelItem.GetVoxelData();
		if(VoxelData.AuxiliaryClass && VoxelData.bMainPart)
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelAuxiliary>(nullptr, VoxelData.AuxiliaryClass))
			{
				AddSceneActor(Auxiliary);
				InVoxelItem.Auxiliary = Auxiliary;
				Auxiliary->Initialize(InVoxelItem);
				return Auxiliary;
			}
		}
	}
	return nullptr;
}

void AVoxelChunk::DestroyAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && InVoxelItem.Auxiliary)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(InVoxelItem.Auxiliary);
		InVoxelItem.Auxiliary = nullptr;
	}
}

AVoxelChunk* AVoxelChunk::GetOrSpawnNeighbor(EDirection InDirection, bool bAddToQueue)
{
	return Neighbors[InDirection] ? Neighbors[InDirection] : AVoxelModule::Get()->SpawnChunk(Index + UMathBPLibrary::DirectionToIndex(InDirection), bAddToQueue);
}

FVector AVoxelChunk::GetChunkLocation() const
{
	return UVoxelModuleBPLibrary::ChunkIndexToLocation(Index);
}
