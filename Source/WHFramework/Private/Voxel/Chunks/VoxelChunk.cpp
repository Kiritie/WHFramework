// Fill out your copyright notice in the Description page of Project Settings.PickUp/


#include "Voxel/Chunks/VoxelChunk.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Character/Base/CharacterBase.h"
#include "Common/CommonStatics.h"
#include "Common/CommonTypes.h"
#include "Debug/DebugModuleTypes.h"
#include "Math/MathStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Voxels/VoxelTree.h"

// Sets default values
AVoxelChunk::AVoxelChunk()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SolidMesh = CreateDefaultSubobject<UVoxelMeshComponent>(FName("SolidMesh"));
	SolidMesh->SetupAttachment(RootComponent);
	SolidMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::Solid);
	SolidMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);

	SemiMesh = CreateDefaultSubobject<UVoxelMeshComponent>(FName("SemiMesh"));
	SemiMesh->SetupAttachment(RootComponent);
	SemiMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::SemiTransparent);
	SemiMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);

	TransMesh = CreateDefaultSubobject<UVoxelMeshComponent>(FName("TransMesh"));
	TransMesh->SetupAttachment(RootComponent);
	TransMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::Transparent);
	TransMesh->OnComponentBeginOverlap.AddDynamic(this, &AVoxelChunk::OnBeginOverlap);
	TransMesh->OnComponentEndOverlap.AddDynamic(this, &AVoxelChunk::OnEndOverlap);

	Batch = -1;
	Index = FIndex::ZeroIndex;
	bBuilded = false;
	bGenerated = false;
	bChanged = false;
	VoxelMap = TMap<FIndex, FVoxelItem>();
	Neighbors = TMap<EDirection, AVoxelChunk*>();
	ITER_DIRECTION(Iter, Neighbors.Add(Iter); )
}

void AVoxelChunk::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void AVoxelChunk::OnDespawn_Implementation(bool bRecovery)
{
	// if(UVoxelModuleStatics::GetWorldMode() == EVoxelWorldMode::Default)
	{
		UVoxelModule::Get().GetWorldData().SetChunkData(Index, GetSaveData<FVoxelChunkSaveData>(true));
	}

	for(auto& Iter : VoxelMap)
	{
		DestroyAuxiliary(Iter.Value);
	}
	
	DestroySceneActors();

	BreakNeighbors();

	Index = FIndex::ZeroIndex;
	Batch = -1;
	bBuilded = false;
	bGenerated = false;
	bChanged = false;

	SolidMesh->ClearData();
	SemiMesh->ClearData();
	TransMesh->ClearData();

	VoxelMap.Empty();

	SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

	Execute_SetActorVisible(this, false);
}

void AVoxelChunk::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();
	
	TArray<FString> VoxelDatas;
	SaveData.VoxelDatas.ParseIntoArray(VoxelDatas, TEXT("/"));
	for(auto& Iter : VoxelDatas)
	{
		FVoxelItem VoxelItem = FVoxelItem(Iter);
		SetVoxelSample(VoxelItem.Index, VoxelItem);
	}
	for(auto& Iter : SaveData.AuxiliaryDatas)
	{
		FVoxelItem& VoxelItem = GetVoxelItem(Iter.VoxelItem.Index);
		VoxelItem.AuxiliaryData = &Iter;
	}
	bBuilded = true;
}

FSaveData* AVoxelChunk::ToData()
{
	static FVoxelChunkSaveData SaveData;
	SaveData = FVoxelChunkSaveData();

	SaveData.Index = Index;
	SaveData.bGenerated = bGenerated;

	for(auto& Iter : VoxelMap)
	{
		if(bChanged)
		{
			SaveData.VoxelDatas.Appendf(TEXT("/%s"), *Iter.Value.ToSaveData(true));
		}
		if(Iter.Value.Auxiliary)
		{
			SaveData.AuxiliaryDatas.Add(Iter.Value.Auxiliary->GetSaveDataRef<FVoxelAuxiliarySaveData>(true));
		}
	}
	
	if(!bChanged && UVoxelModule::Get().GetWorldData().IsExistChunkData(Index))
	{
		SaveData.VoxelDatas = UVoxelModule::Get().GetWorldData().GetChunkData(Index)->VoxelDatas;
	}

	for(auto& Iter : SceneActorMap)
	{
		if(AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(Iter.Value))
		{
			SaveData.PickUpDatas.Add(PickUp->GetSaveDataRef<FPickUpSaveData>(true));
		}
	}

	return &SaveData;
}

void AVoxelChunk::OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherActor && OtherActor->IsA<ACharacterBase>())
	{
		if(IVoxelAgentInterface* VoxelAgent = Cast<IVoxelAgentInterface>(OtherActor))
		{
			const FVoxelItem& VoxelItem = GetVoxelItem(LocationToIndex(Hit.ImpactPoint - UVoxelModule::Get().GetWorldData().GetBlockSizedNormal(Hit.ImpactNormal)), true);
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
			GenerateSceneActors();
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
			const auto& WorldData = UVoxelModule::Get().GetWorldData();
			ITER_INDEX(VoxelIndex, WorldData.ChunkSize, false,
				const auto VoxelType = UVoxelModuleStatics::GetNoiseVoxelType(LocalIndexToWorld(VoxelIndex));
				const FVoxelItem VoxelItem = FVoxelItem(VoxelType);
				switch(VoxelType)
				{
					default:
					{
						SetVoxelSample(VoxelIndex, VoxelItem);
						break;
					}
					case EVoxelType::Empty: break;
				}
			)
			break;
		}
		case 1:
		{
			ITER_MAP(TMap(VoxelMap), Iter,
				switch(Iter.Value.GetVoxelType())
				{
					// Grass
					case EVoxelType::Grass:
					{
						const FIndex VoxelIndex = Iter.Key + FIndex(0, 0, 1);
						const auto VoxelType = UVoxelModuleStatics::GetRandomVoxelType(LocalIndexToWorld(VoxelIndex));
						const FVoxelItem VoxelItem = FVoxelItem(VoxelType, VoxelIndex, this);
						switch(VoxelType)
						{
							// Tree
							case EVoxelType::Oak:
							case EVoxelType::Birch:
							{
								VoxelItem.GetVoxel<UVoxelTree>().BuildData(EVoxelTreePart::Root);
								break;
							}
							default:
							{
								SetVoxelComplex(VoxelIndex, VoxelType);
								break;
							}
							case EVoxelType::Empty: break;
						}
						break;
					}
					default: break;
				}
			)
			break;
		}
		default: break;
	}
	bBuilded = true;
}

void AVoxelChunk::BuildMesh()
{
	ITER_MAP(TMap(VoxelMap), Iter,
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
	);
}

void AVoxelChunk::GenerateNeighbors(FIndex InIndex, EPhase InPhase)
{
	GenerateNeighbors(InIndex.X, InIndex.Y, InIndex.Z, InPhase);
}

void AVoxelChunk::GenerateNeighbors(int32 InX, int32 InY, int32 InZ, EPhase InPhase)
{
	const auto& WorldData = UVoxelModule::Get().GetWorldData();
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
		Neighbors[Direction] = UVoxelModule::Get().FindChunkByIndex(Index + UMathStatics::DirectionToIndex(Direction));
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[UMathStatics::InvertDirection(Direction)] = this;
		}
	)
}

void AVoxelChunk::BreakNeighbors()
{
	ITER_DIRECTION(Direction,
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[UMathStatics::InvertDirection(Direction)] = nullptr;
			Neighbors[Direction] = nullptr;
		}
	)
}

bool AVoxelChunk::IsOnTheChunk(FIndex InIndex) const
{
	const auto& WorldData = UVoxelModule::Get().GetWorldData();
	return InIndex.X >= 0 && InIndex.X < WorldData.ChunkSize.X &&
		InIndex.Y >= 0 && InIndex.Y < WorldData.ChunkSize.Y &&
		InIndex.Z >= 0 && InIndex.Z < WorldData.ChunkSize.Z;
}

bool AVoxelChunk::IsOnTheChunk(FVector InLocation) const
{
	const auto& WorldData = UVoxelModule::Get().GetWorldData();
	return InLocation.X >= GetChunkLocation().X && InLocation.X < GetChunkLocation().X + WorldData.GetChunkRealSize().X &&
		InLocation.Y >= GetChunkLocation().Y && InLocation.Y < GetChunkLocation().Y + WorldData.GetChunkRealSize().Y &&
		InLocation.Z >= GetChunkLocation().Z && InLocation.Z < GetChunkLocation().Z + WorldData.GetChunkRealSize().Z;
}

FIndex AVoxelChunk::LocationToIndex(FVector InLocation, bool bWorldSpace /*= true*/) const
{
	InLocation = (InLocation - (bWorldSpace ? GetChunkLocation() : FVector::ZeroVector)) / UVoxelModule::Get().GetWorldData().BlockSize;
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), FMath::FloorToInt(InLocation.Z));
}

FVector AVoxelChunk::IndexToLocation(FIndex InIndex, bool bWorldSpace /*= true*/) const
{
	return InIndex.ToVector() * UVoxelModule::Get().GetWorldData().BlockSize + (bWorldSpace ? GetChunkLocation() : FVector::ZeroVector);
}

FIndex AVoxelChunk::LocalIndexToWorld(FIndex InIndex) const
{
	return InIndex + Index * UVoxelModule::Get().GetWorldData().ChunkSize;
}

FIndex AVoxelChunk::WorldIndexToLocal(FIndex InIndex) const
{
	return InIndex - Index * UVoxelModule::Get().GetWorldData().ChunkSize;
}

bool AVoxelChunk::LocalIndexToNeighbor(FIndex InIndex, EDirection& OutDirection) const
{
	const auto& WorldData = UVoxelModule::Get().GetWorldData();
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
	const auto& WorldData = UVoxelModule::Get().GetWorldData();
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
	return GetVoxelItem(InIndex, bMainPart).GetVoxel();
}

FVoxelItem& AVoxelChunk::GetVoxelItem(FIndex InIndex, bool bMainPart)
{
	return GetVoxelItem(InIndex.X, InIndex.Y, InIndex.Z, bMainPart);
}

FVoxelItem& AVoxelChunk::GetVoxelItem(int32 InX, int32 InY, int32 InZ, bool bMainPart)
{
	const auto& WorldData = UVoxelModule::Get().GetWorldData();
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
	const FIndex AdjacentIndex = UMathStatics::GetAdjacentIndex(InVoxelItem.Index, InDirection, InVoxelItem.Angle);
	
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
				const FIndex NeighborIndex = InIndex + Iter1 + UMathStatics::DirectionToIndex(Iter2);
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
	if(bSuccess)
	{
		if(bGenerate)
		{
			Generate(EPhase::Lesser);
			GenerateNeighbors(InIndex, EPhase::Lesser);
		}
		if(InAgent)
		{
			bChanged = true;
		}
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
	const auto& WorldData = UVoxelModule::Get().GetWorldData();
	if(InX < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Backward))
		{
			return Chunk->SetVoxelComplex(InX + WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InX >= WorldData.ChunkSize.X)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Forward))
		{
			return Chunk->SetVoxelComplex(InX - WorldData.ChunkSize.X, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Left))
		{
			return Chunk->SetVoxelComplex(InX, InY + WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY >= WorldData.ChunkSize.Y)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Right))
		{
			return Chunk->SetVoxelComplex(InX, InY - WorldData.ChunkSize.Y, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ < 0)
	{
		if(AVoxelChunk* Chunk = GetNeighbor(EDirection::Down))
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
					VoxelItems.Emplace(VoxelIndex, InVoxelItem.IsValid() ? InVoxelItem.ReplaceID(VoxelData.GetPrimaryAssetId()) : FVoxelItem::Empty);
					ITER_INDEX(PartIndex, VoxelRange, false,
						UVoxelData& PartData = VoxelData.GetPartData(PartIndex);
						if(!PartData.bMainPart)
						{
							VoxelItems.Emplace(VoxelIndex + PartIndex, InVoxelItem.IsValid() ? InVoxelItem.ReplaceID(PartData.GetPrimaryAssetId()) : FVoxelItem::Empty);
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
		for(const auto& Iter : neighbors)
		{
			if(GetNeighbor(Iter))
			{
				GetNeighbor(Iter)->Generate(EPhase::Lesser);
			}
		}
	}
	return bSuccess;
}

void AVoxelChunk::SetActorVisible_Implementation(bool bInVisible)
{
	bVisible = bInVisible;
	GetRootComponent()->SetVisibility(bInVisible, true);

	SolidMesh->SetCollisionEnabled(bInVisible);
	SemiMesh->SetCollisionEnabled(bInVisible);
	TransMesh->SetCollisionEnabled(bInVisible);
}

bool AVoxelChunk::HasSceneActor(const FString& InID, bool bEnsured) const
{
	if(SceneActorMap.Contains(FGuid(InID))) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No SceneActor, Actor name: %s"), *InID), EDC_Voxel, EDV_Error);
	return false;
}

AActor* AVoxelChunk::GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if(HasSceneActor(InID, bEnsured))
	{
		return SceneActorMap[FGuid(InID)];
	}
	return nullptr;
}

bool AVoxelChunk::AddSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(!SceneActorMap.Contains(Execute_GetActorID(InActor)))
	{
		SceneActorMap.Add(Execute_GetActorID(InActor), InActor);
		if(Execute_GetContainer(InActor) != this)
		{
			if(Execute_GetContainer(InActor))
			{
				Execute_GetContainer(InActor)->RemoveSceneActor(InActor);
			}
			Execute_SetContainer(InActor, this);
		}
		return true;
	}
	return false;
}

bool AVoxelChunk::RemoveSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(SceneActorMap.Contains(Execute_GetActorID(InActor)))
	{
		SceneActorMap.Remove(Execute_GetActorID(InActor));
		if(Execute_GetContainer(InActor) == this)
		{
			Execute_SetContainer(InActor, nullptr);
		}
		return true;
	}
	return false;
}

void AVoxelChunk::GenerateSceneActors()
{
	auto& WorldData = UVoxelModule::Get().GetWorldData();
	if(WorldData.IsChunkGenerated(Index))
	{
		LoadSceneActors(WorldData.GetChunkData(Index));
	}
	else if(Index.Z == WorldData.WorldSize.Z - 1)
	{
		SpawnSceneActors();
	}
}

void AVoxelChunk::LoadSceneActors(FSaveData* InSaveData)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();
	for(auto& Iter : SaveData.PickUpDatas)
	{
		UAbilityModuleStatics::SpawnAbilityPickUp(&Iter, this);
	}
}

void AVoxelChunk::SpawnSceneActors()
{
}

void AVoxelChunk::DestroySceneActors()
{
	for(auto& Iter : TMap(SceneActorMap))
	{
		if(UCommonStatics::GetPlayerPawn() != Iter.Value)
		{
			UObjectPoolModuleStatics::DespawnObject(Iter.Value);
		}
		SceneActorMap.Remove(Iter.Key);
	}
}

AVoxelAuxiliary* AVoxelChunk::SpawnAuxiliary(FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid() && !InVoxelItem.Auxiliary)
	{
		const auto& VoxelData = InVoxelItem.GetVoxelData();
		if(VoxelData.AuxiliaryClass && VoxelData.bMainPart)
		{
			if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleStatics::SpawnObject<AVoxelAuxiliary>(nullptr, nullptr, false, VoxelData.AuxiliaryClass))
			{
				FVoxelAuxiliarySaveData AuxiliaryData;
				if(InVoxelItem.AuxiliaryData)
				{
					AuxiliaryData = InVoxelItem.AuxiliaryData->CastRef<FVoxelAuxiliarySaveData>();
				}
				AuxiliaryData.VoxelItem = InVoxelItem;
				Auxiliary->LoadSaveData(&AuxiliaryData);
				InVoxelItem.Auxiliary = Auxiliary;
				AddSceneActor(Auxiliary);
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
		UObjectPoolModuleStatics::DespawnObject(InVoxelItem.Auxiliary);
		InVoxelItem.Auxiliary = nullptr;
	}
}

AVoxelChunk* AVoxelChunk::GetOrSpawnNeighbor(EDirection InDirection, bool bAddToQueue)
{
	return Neighbors[InDirection] ? Neighbors[InDirection] : UVoxelModule::Get().SpawnChunk(Index + UMathStatics::DirectionToIndex(InDirection), bAddToQueue);
}

FVector AVoxelChunk::GetChunkLocation() const
{
	return UVoxelModuleStatics::ChunkIndexToLocation(Index);
}
