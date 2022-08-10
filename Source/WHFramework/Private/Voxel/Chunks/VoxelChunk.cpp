// Fill out your copyright notice in the Description page of Project Settings.PickUp/


#include "Voxel/Chunks/VoxelChunk.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/PickUp/AbilityPickUpEquip.h"
#include "Ability/PickUp/AbilityPickUpProp.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Ability/PickUp/AbilityPickUpSkill.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Character/Base/CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Math/MathBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

// Sets default values
AVoxelChunk::AVoxelChunk()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SolidMesh = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("SolidMesh"));
	SolidMesh->SetupAttachment(RootComponent);
	SolidMesh->SetCollisionProfileName(TEXT("SolidVoxel"));
	SolidMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::Solid);
	SolidMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);

	SemiMesh = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("SemiMesh"));
	SemiMesh->SetupAttachment(RootComponent);
	SemiMesh->SetCollisionProfileName(TEXT("SolidVoxel"));
	SemiMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::SemiTransparent);
	SemiMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);

	TransMesh = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("TransMesh"));
	TransMesh->SetupAttachment(RootComponent);
	TransMesh->SetCollisionProfileName(TEXT("TransVoxel"));
	TransMesh->Initialize(EVoxelMeshNature::Chunk, EVoxelTransparency::Transparent);
	TransMesh->OnComponentBeginOverlap.AddDynamic(this, &AVoxelChunk::OnBeginOverlap);
	TransMesh->OnComponentEndOverlap.AddDynamic(this, &AVoxelChunk::OnEndOverlap);

	Batch = -1;
	Module = nullptr;
	Index = FVector();
	bGenerated = false;
	VoxelMap = TMap<FIndex, FVoxelItem>();
	Neighbors = TMap<EDirection, AVoxelChunk*>();
	DIRECTION_ITERATOR(Iter, Neighbors.Add(Iter); )
	PickUps = TArray<AAbilityPickUpBase*>();
}

void AVoxelChunk::BeginPlay()
{
	Super::BeginPlay();
}

void AVoxelChunk::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	auto& SaveData = InSaveData->CastRef<FVoxelChunkSaveData>();
	for(int32 i = 0; i < SaveData.VoxelDatas.Num(); i++)
	{
		FVoxelItem VoxelItem = SaveData.VoxelDatas[i];
		SetVoxelComplex(VoxelItem.Index, VoxelItem);
	}
}

FSaveData* AVoxelChunk::ToData()
{
	static FVoxelChunkSaveData SaveData;
	SaveData.Reset();

	SaveData.Index = Index;

	for(auto& Iter : VoxelMap)
	{
		SaveData.VoxelDatas.Add(Iter.Value.ToSaveData(true));
	}

	for(auto& Iter : PickUps)
	{
		SaveData.PickUpDatas.Add(Iter->ToSaveDataRef<FPickUpSaveData>());
	}

	return &SaveData;
}

void AVoxelChunk::OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherActor && OtherActor->IsA<ACharacterBase>())
	{
		if(IVoxelAgentInterface* voxelAgent = Cast<IVoxelAgentInterface>(OtherActor))
		{
			const FVoxelItem& voxelItem = GetVoxelItem(LocationToIndex(Hit.ImpactPoint - UVoxelModuleBPLibrary::GetWorldData().GetBlockSizedNormal(Hit.ImpactNormal)));
			if(voxelItem.IsValid())
			{
				voxelItem.GetVoxel().OnAgentHit(voxelAgent, FVoxelHitResult(voxelItem, Hit.ImpactPoint, Hit.ImpactNormal));
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

// Called every frame
void AVoxelChunk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AVoxelChunk::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
}

void AVoxelChunk::OnDespawn_Implementation()
{
	for(auto& Iter : VoxelMap)
	{
		DestroyAuxiliary(Iter.Key);
	}
	
	DestroyActors();

	BreakNeighbors();

	Module = nullptr;
	Index = FIndex::ZeroIndex;
	Batch = -1;
	bGenerated = false;

	SolidMesh->ClearData();
	SemiMesh->ClearData();
	TransMesh->ClearData();

	VoxelMap.Empty();

	Execute_SetActorVisible(this, false);
}

void AVoxelChunk::SetActorVisible_Implementation(bool bNewVisible)
{
	Super::SetActorVisible_Implementation(bNewVisible);

	if(!bNewVisible)
	{
		SolidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SemiMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TransMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		SolidMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SemiMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TransMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void AVoxelChunk::Initialize(AVoxelModule* InModule, FIndex InIndex, int32 InBatch)
{
	Module = InModule;
	Index = InIndex;
	Batch = InBatch;
	UpdateNeighbors();
}

void AVoxelChunk::Generate(bool bForceMode)
{
	if(bGenerated)
	{
		BuildMesh();
	}
	
	if(!SolidMesh->IsEmpty())
	{
		SolidMesh->CreateMesh();
	}
	else
	{
		SolidMesh->ClearMesh();
	}
	
	if(!SemiMesh->IsEmpty())
	{
		SemiMesh->CreateMesh();
	}
	else
	{
		SemiMesh->ClearMesh();
	}
	
	if(!TransMesh->IsEmpty())
	{
		TransMesh->CreateMesh();
	}
	else
	{
		TransMesh->ClearMesh();
	}

	Execute_SetActorVisible(this, true);

	for(auto& Iter : VoxelMap)
	{
		SpawnAuxiliary(Iter.Key);
		Iter.Value.GetVoxel().OnGenerate(nullptr);
	}

	if(bForceMode)
	{
		SpawnActors();
	}
	
	bGenerated = true;
}

void AVoxelChunk::BuildMap(int32 InStage)
{
	switch (InStage)
	{
		case 0:
		{
			for(int32 x = 0; x < UVoxelModuleBPLibrary::GetWorldData().ChunkSize; x++)
			{
				for(int32 y = 0; y < UVoxelModuleBPLibrary::GetWorldData().ChunkSize; y++)
				{
					for(int32 z = 0; z < UVoxelModuleBPLibrary::GetWorldData().ChunkSize; z++)
					{
						FIndex voxelIndex = FIndex(x, y, z);

						if(HasVoxel(voxelIndex)) continue;

						SetVoxelSample(voxelIndex, FVoxelItem(Module->GetNoiseVoxelType(LocalIndexToWorld(voxelIndex))));
					}
				}
			}
			break;
		}
		case 1:
		{
			TArray<FVoxelItem> voxelItems;
			VoxelMap.GenerateValueArray(voxelItems);
			for(auto iter : voxelItems)
			{
				const FIndex voxelIndex = iter.Index;
				switch(iter.GetVoxelType())
				{
					// grass
					case EVoxelType::Grass:
					{
						const float tmpNum = FMath::FRandRange(0.f, 1.f);
						// plant
						if(tmpNum < 0.2f)
						{
							SetVoxelComplex(FIndex(voxelIndex.X, voxelIndex.Y, voxelIndex.Z + 1), FVoxelItem(FMath::FRandRange(0.f, 1.f) > 0.2f ? EVoxelType::Tall_Grass : (EVoxelType)FMath::RandRange((int32)EVoxelType::Flower_Allium, (int32)EVoxelType::Flower_Tulip_White)));
						}
						// tree
						else if(tmpNum < 0.21f)
						{
							if((voxelIndex.X > 2 && voxelIndex.X <= UVoxelModuleBPLibrary::GetWorldData().ChunkSize - 2) && (voxelIndex.Y > 2 && voxelIndex.Y <= UVoxelModuleBPLibrary::GetWorldData().ChunkSize - 2))
							{
								const int32 treeHeight = FMath::RandRange(4.f, 5.f);
								const int32 leavesHeight = 2/*FMath::RandRange(2, 2)*/;
								const int32 leavesWidth = FMath::FRandRange(0.f, 1.f) < 0.5f ? 3 : 5;
								for(int32 trunkHeight = 0; trunkHeight < treeHeight; trunkHeight++)
								{
									SetVoxelComplex(FIndex(voxelIndex.X, voxelIndex.Y, voxelIndex.Z + trunkHeight + 1), FVoxelItem(EVoxelType::Oak));
								}
								for(int32 offsetZ = treeHeight - leavesHeight; offsetZ < treeHeight + 1; offsetZ++)
								{
									for(int32 offsetX = -leavesWidth / 2; offsetX <= leavesWidth / 2; offsetX++)
									{
										for(int32 offsetY = -leavesWidth / 2; offsetY <= leavesWidth / 2; offsetY++)
										{
											SetVoxelComplex(FIndex(voxelIndex.X + offsetX, voxelIndex.Y + offsetY, voxelIndex.Z + offsetZ + 1), FVoxelItem(EVoxelType::Oak_Leaves));
										}
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
	for(auto& iter : VoxelMap)
	{
		const FVoxelItem& voxelItem = iter.Value;
		if(voxelItem.IsValid())
		{
			switch(voxelItem.GetData<UVoxelData>().Transparency)
			{
				case EVoxelTransparency::Solid:
				{
					SolidMesh->BuildVoxel(voxelItem);
					break;
				}
				case EVoxelTransparency::SemiTransparent:
				{
					SemiMesh->BuildVoxel(voxelItem);
					break;
				}
				case EVoxelTransparency::Transparent:
				{
					TransMesh->BuildVoxel(voxelItem);
					break;
				}
				default: break;
			}
		}
	}
}


void AVoxelChunk::SpawnActors()
{
	if(UVoxelModuleBPLibrary::GetWorldData().IsExistChunkData(Index))
	{
		auto chunkData = *UVoxelModuleBPLibrary::GetWorldData().GetChunkData<FVoxelChunkSaveData>(Index);
		for(int32 i = 0; i < chunkData.PickUpDatas.Num(); i++)
		{
			UAbilityModuleBPLibrary::SpawnPickUp(&chunkData.PickUpDatas[i], this);
		}
	}
}

void AVoxelChunk::DestroyActors()
{
	for(auto& Iter : PickUps)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(Iter);
	}
	PickUps.Empty();
}

void AVoxelChunk::GenerateNeighbors(FIndex InIndex, bool bForceMode)
{
	GenerateNeighbors(InIndex.X, InIndex.Y, InIndex.Z, bForceMode);
}

void AVoxelChunk::GenerateNeighbors(int32 InX, int32 InY, int32 InZ, bool bForceMode)
{
	if(InX <= 0 && Neighbors[EDirection::Back])
	{
		Neighbors[EDirection::Back]->Generate(bForceMode);
	}
	else if(InX >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize - 1 && Neighbors[EDirection::Forward])
	{
		Neighbors[EDirection::Forward]->Generate(bForceMode);
	}
	if(InY <= 0 && Neighbors[EDirection::Left])
	{
		Neighbors[EDirection::Left]->Generate(bForceMode);
	}
	else if(InY >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize - 1 && Neighbors[EDirection::Right])
	{
		Neighbors[EDirection::Right]->Generate(bForceMode);
	}
	if(InZ <= 0 && Neighbors[EDirection::Down])
	{
		Neighbors[EDirection::Down]->Generate(bForceMode);
	}
	else if(InZ >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize - 1 && Neighbors[EDirection::Up])
	{
		Neighbors[EDirection::Up]->Generate(bForceMode);
	}
}

void AVoxelChunk::UpdateNeighbors()
{
	DIRECTION_ITERATOR(Direction,
		Neighbors[Direction] = Module->FindChunkByIndex(Index + UMathBPLibrary::DirectionToIndex(Direction));
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[UMathBPLibrary::InvertDirection((EDirection)Direction)] = this;
		}
	)
}

void AVoxelChunk::BreakNeighbors()
{
	DIRECTION_ITERATOR(Direction,
		if(Neighbors[Direction])
		{
			Neighbors[Direction]->Neighbors[UMathBPLibrary::InvertDirection(Direction)] = nullptr;
			Neighbors[Direction] = nullptr;
		}
	)
}

bool AVoxelChunk::IsOnTheChunk(FIndex InIndex) const
{
	return InIndex.X >= 0 && InIndex.X < UVoxelModuleBPLibrary::GetWorldData().ChunkSize &&
		InIndex.Y >= 0 && InIndex.Y < UVoxelModuleBPLibrary::GetWorldData().ChunkSize &&
		InIndex.Z >= 0 && InIndex.Z < UVoxelModuleBPLibrary::GetWorldData().ChunkSize;
}

bool AVoxelChunk::IsOnTheChunk(FVector InLocation) const
{
	return InLocation.X >= GetActorLocation().X && InLocation.X < GetActorLocation().X + UVoxelModuleBPLibrary::GetWorldData().GetChunkLength() &&
		InLocation.Y >= GetActorLocation().Y && InLocation.Y < GetActorLocation().Y + UVoxelModuleBPLibrary::GetWorldData().GetChunkLength() &&
		InLocation.Z >= GetActorLocation().Z && InLocation.Z < GetActorLocation().Z + UVoxelModuleBPLibrary::GetWorldData().GetChunkLength();
}

FIndex AVoxelChunk::LocationToIndex(FVector InLocation, bool bWorldSpace /*= true*/) const
{
	const FVector point = (!bWorldSpace ? InLocation : GetActorTransform().InverseTransformPosition(InLocation)) / UVoxelModuleBPLibrary::GetWorldData().BlockSize;

	FIndex index;
	index.X = FMath::FloorToInt(point.X);
	index.Y = FMath::FloorToInt(point.Y);
	index.Z = FMath::FloorToInt(point.Z);

	return index;
}

FVector AVoxelChunk::IndexToLocation(FIndex InIndex, bool bWorldSpace /*= true*/) const
{
	const FVector localPoint = InIndex.ToVector() * UVoxelModuleBPLibrary::GetWorldData().BlockSize;
	if(!bWorldSpace) return localPoint;
	return GetActorTransform().TransformPosition(localPoint);
}

FIndex AVoxelChunk::LocalIndexToWorld(FIndex InIndex) const
{
	return InIndex + Index * UVoxelModuleBPLibrary::GetWorldData().ChunkSize;
}

FIndex AVoxelChunk::WorldIndexToLocal(FIndex InIndex) const
{
	return InIndex - Index * UVoxelModuleBPLibrary::GetWorldData().ChunkSize;
}

bool AVoxelChunk::HasVoxel(FIndex InIndex)
{
	return VoxelMap.Contains(InIndex);
}

bool AVoxelChunk::HasVoxel(int32 InX, int32 InY, int32 InZ)
{
	return HasVoxel(FIndex(InX, InY, InZ));
}

UVoxel& AVoxelChunk::GetVoxel(FIndex InIndex)
{
	return GetVoxel(InIndex.X, InIndex.Y, InIndex.Z);
}

UVoxel& AVoxelChunk::GetVoxel(int32 InX, int32 InY, int32 InZ)
{
	return GetVoxelItem(InX, InY, InZ).GetVoxel();
}

FVoxelItem& AVoxelChunk::GetVoxelItem(FIndex InIndex)
{
	return GetVoxelItem(InIndex.X, InIndex.Y, InIndex.Z);
}

FVoxelItem& AVoxelChunk::GetVoxelItem(int32 InX, int32 InY, int32 InZ)
{
	if(InX < 0)
	{
		if(Neighbors[EDirection::Back])
		{
			return Neighbors[EDirection::Back]->GetVoxelItem(InX + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InY, InZ);
		}
		return FVoxelItem::Unknown;
	}
	else if(InX >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[EDirection::Forward])
		{
			return Neighbors[EDirection::Forward]->GetVoxelItem(InX - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InY, InZ);
		}
		return FVoxelItem::Unknown;
	}
	else if(InY < 0)
	{
		if(Neighbors[EDirection::Left])
		{
			return Neighbors[EDirection::Left]->GetVoxelItem(InX, InY + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InZ);
		}
		return FVoxelItem::Unknown;
	}
	else if(InY >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[EDirection::Right])
		{
			return Neighbors[EDirection::Right]->GetVoxelItem(InX, InY - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InZ);
		}
		return FVoxelItem::Unknown;
	}
	else if(InZ < 0)
	{
		if(Neighbors[EDirection::Down])
		{
			return Neighbors[EDirection::Down]->GetVoxelItem(InX, InY, InZ + UVoxelModuleBPLibrary::GetWorldData().ChunkSize);
		}
		else if(Index.Z > 0)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(InZ >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[EDirection::Up])
		{
			return Neighbors[EDirection::Up]->GetVoxelItem(InX, InY, InZ - UVoxelModuleBPLibrary::GetWorldData().ChunkSize);
		}
		else if(Index.Z < UVoxelModuleBPLibrary::GetWorldData().ChunkHeightRange)
		{
			return FVoxelItem::Unknown;
		}
	}
	else if(HasVoxel(FIndex(InX, InY, InZ)))
	{
		return VoxelMap[FIndex(InX, InY, InZ)];
	}
	return FVoxelItem::Empty;
}

bool AVoxelChunk::CheckVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, FVector InRange/* = FVector::OneVector*/)
{
	INDEX_ITERATOR(Iter, InRange,
		if(!GetVoxelItem(InIndex + Iter).IsReplaceable(InVoxelItem))
		{
			return true;
		}
	)
	return false;
}

bool AVoxelChunk::CheckVoxelAdjacent(FIndex InIndex, EDirection InDirection)
{
	const FVoxelItem& voxelItem = GetVoxelItem(InIndex);
	const FIndex adjacentIndex = UMathBPLibrary::GetAdjacentIndex(InIndex, InDirection, voxelItem.Rotation);
	
	if(!voxelItem.IsValid() || LocalIndexToWorld(adjacentIndex).Z <= 0) return true;
	
	const FVoxelItem& adjacentItem = GetVoxelItem(adjacentIndex);
	if(adjacentItem.IsValid())
	{
		const UVoxelData& voxelData = voxelItem.GetData<UVoxelData>();
		const UVoxelData& adjacentData = adjacentItem.GetData<UVoxelData>();
		switch(voxelData.Transparency)
		{
			case EVoxelTransparency::Solid:
			{
				switch(adjacentData.Transparency)
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
				switch(adjacentData.Transparency)
				{
					case EVoxelTransparency::SemiTransparent:
					{
						if(voxelData.VoxelType == adjacentData.VoxelType)
						{
							switch(voxelData.VoxelType)
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
				switch(adjacentData.Transparency)
				{
					case EVoxelTransparency::Solid:
					case EVoxelTransparency::SemiTransparent:
					{
						return true;
					}
					case EVoxelTransparency::Transparent:
					{
						if(voxelData.VoxelType == adjacentData.VoxelType)
						{
							switch(voxelData.VoxelType)
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
	else if(adjacentItem.IsUnknown())
	{
		return true;
	}
	return false;
}

bool AVoxelChunk::CheckVoxelNeighbors(FIndex InIndex, EVoxelType InVoxelType, FVector InRange, bool bIgnoreBottom)
{
	INDEX_ITERATOR(Iter1, InRange,
		DIRECTION_ITERATOR(Iter2, 
			if(!bIgnoreBottom || Iter2 != EDirection::Down)
			{
				FVoxelItem& NeighborItem = GetVoxelItem(InIndex + Iter1 + UMathBPLibrary::DirectionToIndex(Iter2));
				if(NeighborItem.IsValid() && NeighborItem.GetData<UVoxelData>().VoxelType == InVoxelType)
				{
					return true;
				}
			}
		)
	)
	return false;
}

bool AVoxelChunk::SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate, IVoxelAgentInterface* InAgent)
{
	bool ReturnValue = false;
	
	if(InVoxelItem.IsValid())
	{
		if(!CheckVoxel(InIndex, InVoxelItem))
		{
			FVoxelItem VoxelItem = FVoxelItem(InVoxelItem);
			VoxelItem.Index = InIndex;
			VoxelItem.Owner = this;
			VoxelMap.Emplace(InIndex, VoxelItem);
			if(bGenerate)
			{
				SpawnAuxiliary(InIndex);
				GetVoxel(InIndex).OnGenerate(InAgent);
			}
			ReturnValue = true;
		}
	}
	else if(HasVoxel(InIndex))
	{
		if(bGenerate)
		{
			GetVoxel(InIndex).OnDestroy(InAgent);
			DestroyAuxiliary(InIndex);
		}
		VoxelMap.Remove(InIndex);
		ReturnValue = true;
	}

	if(ReturnValue && bGenerate)
	{
		Generate();
		GenerateNeighbors(InIndex);
	}

	return ReturnValue;
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
	if(InX < 0)
	{
		if(Neighbors[EDirection::Back])
		{
			return Neighbors[EDirection::Back]->SetVoxelComplex(InX + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InX >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[EDirection::Forward])
		{
			return Neighbors[EDirection::Forward]->SetVoxelComplex(InX - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InY, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY < 0)
	{
		if(Neighbors[EDirection::Left])
		{
			return Neighbors[EDirection::Left]->SetVoxelComplex(InX, InY + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InY >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[EDirection::Right])
		{
			return Neighbors[EDirection::Right]->SetVoxelComplex(InX, InY - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InZ, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ < 0)
	{
		if(Neighbors[EDirection::Down])
		{
			return Neighbors[EDirection::Down]->SetVoxelComplex(InX, InY, InZ + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InVoxelItem, bGenerate, InAgent);
		}
	}
	else if(InZ >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(!Neighbors[EDirection::Up])
		{
			Module->SpawnChunk(Index + UMathBPLibrary::DirectionToIndex(EDirection::Up), !bGenerate);
		}
		if(Neighbors[EDirection::Up])
		{
			return Neighbors[EDirection::Up]->SetVoxelComplex(InX, InY, InZ - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InVoxelItem, bGenerate, InAgent);
		}
	}
	else
	{
		const FIndex index = FIndex(InX, InY, InZ);
		UVoxelData& voxelData = InVoxelItem.IsValid() ? InVoxelItem.GetData<UVoxelData>() : GetVoxelItem(InX, InY, InZ).GetData<UVoxelData>();
		if(voxelData.PartType == EVoxelPartType::Main)
		{
			const FVector range = voxelData.GetRange(InVoxelItem.Rotation);
			if(range != FVector::OneVector)
			{
				if(!CheckVoxel(FIndex(InX, InY, InZ), InVoxelItem, range))
				{
					INDEX_ITERATOR(partIndex, range,
						const UVoxelData& partData = voxelData.GetPartData(partIndex);
						if(partData.IsValid())
						{
							const EVoxelType voxelType = InVoxelItem.IsValid() ? partData.VoxelType : EVoxelType::Empty;
							if(partIndex == FIndex::ZeroIndex)
							{
								SetVoxelSample(index, voxelType, bGenerate, InAgent);
							}
							else
							{
								SetVoxelComplex(index+ partIndex, voxelType, bGenerate, InAgent);
							}
						}
					)
					return true;
				}
			}
			else
			{
				return SetVoxelSample(index, InVoxelItem, bGenerate, InAgent);
			}
		}
		else
		{
			return SetVoxelSample(index, InVoxelItem, bGenerate, InAgent);
		}
	}
	return false;
}

void AVoxelChunk::AddSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>() || ISceneActorInterface::Execute_GetContainer(InActor) == this) return;

	if(ISceneActorInterface::Execute_GetContainer(InActor))
	{
		ISceneActorInterface::Execute_GetContainer(InActor)->RemoveSceneActor(InActor);
	}

	if(AVoxelAuxiliary* Auxiliary = Cast<AVoxelAuxiliary>(InActor))
	{
		const FIndex VoxelIndex = Auxiliary->GetVoxelIndex();
		if(HasVoxel(VoxelIndex) && VoxelMap[VoxelIndex].Auxiliary != Auxiliary)
		{
			VoxelMap[VoxelIndex].Auxiliary = Auxiliary;
			Auxiliary->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			Auxiliary->Execute_SetContainer(Auxiliary, this);
		}
	}

	if(AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(InActor))
	{
		if(!PickUps.Contains(PickUp))
		{
			PickUp->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			PickUp->Execute_SetContainer(PickUp, this);
			PickUps.Add(PickUp);
		}
	}
}

void AVoxelChunk::RemoveSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>() || ISceneActorInterface::Execute_GetContainer(InActor) != this) return;

	if(AVoxelAuxiliary* Auxiliary = Cast<AVoxelAuxiliary>(InActor))
	{
		const FIndex VoxelIndex = Auxiliary->GetVoxelIndex();
		if(HasVoxel(VoxelIndex) && VoxelMap[VoxelIndex].Auxiliary == Auxiliary)
		{
			VoxelMap[VoxelIndex].Auxiliary = nullptr;
			Auxiliary->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Auxiliary->Execute_SetContainer(Auxiliary, nullptr);
		}
	}

	if(AAbilityPickUpBase* PickUp = Cast<AAbilityPickUpBase>(InActor))
	{
		PickUp->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		PickUp->Execute_SetContainer(PickUp, nullptr);
		PickUps.Remove(PickUp);
	}
}

AVoxelAuxiliary* AVoxelChunk::SpawnAuxiliary(FIndex InIndex)
{
	const auto& VoxelItem = GetVoxelItem(InIndex);
	const auto& VoxelData = VoxelItem.GetData<UVoxelData>();
	if(!VoxelItem.Auxiliary && VoxelData.AuxiliaryClass)
	{
		const FVector Location = VoxelItem.GetLocation() + VoxelData.GetRange() * UVoxelModuleBPLibrary::GetWorldData().BlockSize * 0.5f;
		if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelAuxiliary>(nullptr, VoxelData.AuxiliaryClass))
		{
			Auxiliary->SetActorLocationAndRotation(Location, VoxelItem.Rotation);
			Auxiliary->Initialize(this, VoxelItem.Index);
			AddSceneActor(Auxiliary);
			return Auxiliary;
		}
	}
	return nullptr;
}

void AVoxelChunk::DestroyAuxiliary(FIndex InIndex)
{
	auto& VoxelItem = GetVoxelItem(InIndex);
	if(VoxelItem.Auxiliary)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(VoxelItem.Auxiliary);
		VoxelItem.Auxiliary = nullptr;
	}
}

bool AVoxelChunk::IsEntity() const
{
	return !SolidMesh->IsEmpty() || !SemiMesh->IsEmpty();
}
