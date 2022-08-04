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
	SolidMesh->Initialize(EVoxelMeshType::Chunk, EVoxelTransparency::Solid);
	SolidMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);

	SemiMesh = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("SemiMesh"));
	SemiMesh->SetupAttachment(RootComponent);
	SemiMesh->SetCollisionProfileName(TEXT("SolidVoxel"));
	SemiMesh->Initialize(EVoxelMeshType::Chunk, EVoxelTransparency::SemiTransparent);
	SemiMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);

	TransMesh = CreateDefaultSubobject<UVoxelMeshComponent>(TEXT("TransMesh"));
	TransMesh->SetupAttachment(RootComponent);
	TransMesh->SetCollisionProfileName(TEXT("TransVoxel"));
	TransMesh->Initialize(EVoxelMeshType::Chunk, EVoxelTransparency::Transparent);
	TransMesh->OnComponentBeginOverlap.AddDynamic(this, &AVoxelChunk::OnBeginOverlap);
	TransMesh->OnComponentEndOverlap.AddDynamic(this, &AVoxelChunk::OnEndOverlap);

	Batch = -1;
	Module = nullptr;
	Index = FVector();
	bGenerated = false;
	VoxelMap = TMap<FIndex, FVoxelItem>();
	Neighbors = TArray<AVoxelChunk*>();
	Neighbors.SetNumZeroed(6);
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
		Iter.Value.RefreshSaveData();
		SaveData.VoxelDatas.Add(Iter.Value);
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
		DestroyAuxiliary(Iter.Value);
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
		GenerateMap();
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
		SpawnAuxiliary(Iter.Value);
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

						if(VoxelMap.Contains(voxelIndex)) continue;

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
				switch(iter.VoxelType)
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
											const FVoxelItem& voxelItem = GetVoxelItem(voxelIndex.X + offsetX, voxelIndex.Y + offsetY, voxelIndex.Z + offsetZ + 1);
											if(!voxelItem.IsValid() || voxelItem.GetData<UVoxelData>().Transparency == EVoxelTransparency::Transparent)
											{
												SetVoxelComplex(FIndex(voxelIndex.X + offsetX, voxelIndex.Y + offsetY, voxelIndex.Z + offsetZ + 1), FVoxelItem(EVoxelType::Oak_Leaves));
											}
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

void AVoxelChunk::GenerateMap()
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
	if(InX <= 0 && Neighbors[(int32)EDirection::Back] != nullptr)
	{
		Neighbors[(int32)EDirection::Back]->Generate(bForceMode);
	}
	else if(InX >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize - 1 && Neighbors[(int32)EDirection::Forward] != nullptr)
	{
		Neighbors[(int32)EDirection::Forward]->Generate(bForceMode);
	}
	if(InY <= 0 && Neighbors[(int32)EDirection::Left] != nullptr)
	{
		Neighbors[(int32)EDirection::Left]->Generate(bForceMode);
	}
	else if(InY >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize - 1 && Neighbors[(int32)EDirection::Right] != nullptr)
	{
		Neighbors[(int32)EDirection::Right]->Generate(bForceMode);
	}
	if(InZ <= 0 && Neighbors[(int32)EDirection::Down] != nullptr)
	{
		Neighbors[(int32)EDirection::Down]->Generate(bForceMode);
	}
	else if(InZ >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize - 1 && Neighbors[(int32)EDirection::Up] != nullptr)
	{
		Neighbors[(int32)EDirection::Up]->Generate(bForceMode);
	}
}

void AVoxelChunk::UpdateNeighbors()
{
	for(int32 i = 0; i < 6; i++)
	{
		Neighbors[i] = Module->FindChunkByIndex(Index + UMathBPLibrary::DirectionToIndex((EDirection)i));
		if(Neighbors[i] != nullptr)
		{
			Neighbors[i]->Neighbors[(int32)UMathBPLibrary::InvertDirection((EDirection)i)] = this;
		}
	}
}

void AVoxelChunk::BreakNeighbors()
{
	for(int32 i = 0; i < 6; i++)
	{
		if(Neighbors[i] != nullptr)
		{
			Neighbors[i]->Neighbors[(int32)UMathBPLibrary::InvertDirection((EDirection)i)] = nullptr;
			Neighbors[i] = nullptr;
		}
	}
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
		if(Neighbors[(int32)EDirection::Back] != nullptr)
			return Neighbors[(int32)EDirection::Back]->GetVoxelItem(InX + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InY, InZ);
		return FVoxelItem::Unknown;
	}
	else if(InX >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[(int32)EDirection::Forward] != nullptr)
			return Neighbors[(int32)EDirection::Forward]->GetVoxelItem(InX - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InY, InZ);
		return FVoxelItem::Unknown;
	}
	else if(InY < 0)
	{
		if(Neighbors[(int32)EDirection::Left] != nullptr)
			return Neighbors[(int32)EDirection::Left]->GetVoxelItem(InX, InY + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InZ);
		return FVoxelItem::Unknown;
	}
	else if(InY >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[(int32)EDirection::Right] != nullptr)
			return Neighbors[(int32)EDirection::Right]->GetVoxelItem(InX, InY - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InZ);
		return FVoxelItem::Unknown;
	}
	else if(InZ < 0)
	{
		if(Neighbors[(int32)EDirection::Down] != nullptr)
			return Neighbors[(int32)EDirection::Down]->GetVoxelItem(InX, InY, InZ + UVoxelModuleBPLibrary::GetWorldData().ChunkSize);
		else if(Index.Z > 0)
			return FVoxelItem::Unknown;
	}
	else if(InZ >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[(int32)EDirection::Up] != nullptr)
			return Neighbors[(int32)EDirection::Up]->GetVoxelItem(InX, InY, InZ - UVoxelModuleBPLibrary::GetWorldData().ChunkSize);
		else if(Index.Z < UVoxelModuleBPLibrary::GetWorldData().ChunkHeightRange)
			return FVoxelItem::Unknown;
	}
	else if(VoxelMap.Contains(FIndex(InX, InY, InZ)))
	{
		return VoxelMap[FIndex(InX, InY, InZ)];
	}
	return FVoxelItem::Empty;
}

bool AVoxelChunk::CheckVoxel(FIndex InIndex, FVector InRange/* = FVector::OneVector*/, bool bIgnoreTransparent /*= true*/)
{
	if(InRange != FVector::OneVector)
	{
		for(int32 x = 0; x < InRange.X; x++)
		{
			for(int32 y = 0; y < InRange.Y; y++)
			{
				for(int32 z = 0; z < InRange.Z; z++)
				{
					const FVoxelItem& voxelItem = GetVoxelItem(InIndex + FIndex(x, y, z));
					if(voxelItem.IsValid() && (!bIgnoreTransparent || voxelItem.GetData<UVoxelData>().Transparency != EVoxelTransparency::Transparent))
					{
						return true;
					}
				}
			}
		}
	}
	else
	{
		const FVoxelItem& voxelItem = GetVoxelItem(InIndex);
		if(voxelItem.IsValid() && (!bIgnoreTransparent || voxelItem.GetData<UVoxelData>().Transparency != EVoxelTransparency::Transparent))
		{
			return true;
		}
	}
	return false;
}

bool AVoxelChunk::CheckVoxel(int32 InX, int32 InY, int32 InZ, FVector InRange/* = FVector::OneVector*/, bool bIgnoreTransparent /*= true*/)
{
	return CheckVoxel(FIndex(InX, InY, InZ), InRange, bIgnoreTransparent);
}

bool AVoxelChunk::CheckAdjacent(const FVoxelItem& InVoxelItem, EDirection InDirection)
{
	if(InDirection == EDirection::Down && LocalIndexToWorld(InVoxelItem.Index).Z == 0) return true;

	const auto& voxelData = InVoxelItem.GetData<UVoxelData>();

	FVoxelItem& adjacentItem = GetVoxelItem(UMathBPLibrary::GetAdjacentIndex(InVoxelItem.Index, InDirection, InVoxelItem.Rotation));

	if(adjacentItem.IsValid())
	{
		UVoxelData& adjacentData = adjacentItem.GetData<UVoxelData>();
		if(voxelData.Range * InVoxelItem.Scale == adjacentData.Range * adjacentItem.Scale)
		{
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
	}
	else if(adjacentItem.IsUnknown())
	{
		return false;
	}
	return false;
}

bool AVoxelChunk::CheckNeighbors(const FVoxelItem& InVoxelItem, EVoxelType InVoxelType, bool bIgnoreBottom, int32 InDistance)
{
	const FVector range = InVoxelItem.GetData<UVoxelData>().GetCeilRange(InVoxelItem.Rotation, InVoxelItem.Scale);
	for(int32 x = -InDistance; x < range.X + InDistance; x++)
	{
		for(int32 y = -InDistance; y < range.Y + InDistance; y++)
		{
			for(int32 z = bIgnoreBottom ? 0 : -InDistance; z < range.Z + InDistance; z++)
			{
				FVoxelItem& neighborItem = GetVoxelItem(Index + FIndex(x, y, z));
				if(neighborItem.IsValid() && neighborItem.GetData<UVoxelData>().VoxelType == InVoxelType)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool AVoxelChunk::SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerateMesh)
{
	bool bSuccess = false;

	if(InVoxelItem.IsValid())
	{
		if(!CheckVoxel(InIndex))
		{
			FVoxelItem VoxelItem = FVoxelItem(InVoxelItem);
			VoxelItem.Index = InIndex;
			VoxelItem.Owner = this;
			VoxelMap.Emplace(InIndex, VoxelItem);
			bSuccess = true;
		}
	}
	else if(VoxelMap.Contains(InIndex))
	{
		VoxelMap.Remove(InIndex);
		bSuccess = true;
	}

	if(bSuccess && bGenerateMesh)
	{
		Generate();
		GenerateNeighbors(InIndex);
	}

	return bSuccess;
}

bool AVoxelChunk::SetVoxelSample(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerateMesh)
{
	return SetVoxelSample(FIndex(InX, InY, InZ), InVoxelItem, bGenerateMesh);
}

bool AVoxelChunk::SetVoxelComplex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerateMesh)
{
	return SetVoxelComplex(InIndex.X, InIndex.Y, InIndex.Z, InVoxelItem, bGenerateMesh);
}

bool AVoxelChunk::SetVoxelComplex(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerateMesh)
{
	if(InX < 0)
	{
		if(Neighbors[(int32)EDirection::Back] != nullptr)
			return Neighbors[(int32)EDirection::Back]->SetVoxelComplex(InX + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InY, InZ, InVoxelItem, bGenerateMesh);
	}
	else if(InX >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[(int32)EDirection::Forward] != nullptr)
			return Neighbors[(int32)EDirection::Forward]->SetVoxelComplex(InX - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InY, InZ, InVoxelItem, bGenerateMesh);
	}
	else if(InY < 0)
	{
		if(Neighbors[(int32)EDirection::Left] != nullptr)
			return Neighbors[(int32)EDirection::Left]->SetVoxelComplex(InX, InY + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InZ, InVoxelItem, bGenerateMesh);
	}
	else if(InY >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[(int32)EDirection::Right] != nullptr)
			return Neighbors[(int32)EDirection::Right]->SetVoxelComplex(InX, InY - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InZ, InVoxelItem, bGenerateMesh);
	}
	else if(InZ < 0)
	{
		if(Neighbors[(int32)EDirection::Down] != nullptr)
			return Neighbors[(int32)EDirection::Down]->SetVoxelComplex(InX, InY, InZ + UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InVoxelItem, bGenerateMesh);
	}
	else if(InZ >= UVoxelModuleBPLibrary::GetWorldData().ChunkSize)
	{
		if(Neighbors[(int32)EDirection::Up] == nullptr)
			Module->SpawnChunk(Index + UMathBPLibrary::DirectionToIndex(EDirection::Up), !bGenerateMesh);
		if(Neighbors[(int32)EDirection::Up] != nullptr)
			return Neighbors[(int32)EDirection::Up]->SetVoxelComplex(InX, InY, InZ - UVoxelModuleBPLibrary::GetWorldData().ChunkSize, InVoxelItem, bGenerateMesh);
	}
	else
	{
		const UVoxelData& voxelData = InVoxelItem.IsValid() ? InVoxelItem.GetData<UVoxelData>() : GetVoxelItem(InX, InY, InZ).GetData<UVoxelData>();
		if(voxelData.IsComplex())
		{
			const FVector range = voxelData.GetCeilRange(InVoxelItem.Rotation, InVoxelItem.Scale);
			if(!CheckVoxel(InX, InY, InZ, range))
			{
				for(int32 x = 0; x < range.X; x++)
				{
					for(int32 y = 0; y < range.Y; y++)
					{
						for(int32 z = 0; z < range.Z; z++)
						{
							const FIndex index = FIndex(InX + x, InY + y, InZ + z);
							SetVoxelSample(index, InVoxelItem, bGenerateMesh);
						}
					}
				}
				return true;
			}
		}
		else
		{
			return SetVoxelSample(InX, InY, InZ, InVoxelItem, bGenerateMesh);
		}
	}
	return false;
}

bool AVoxelChunk::GenerateVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, IVoxelAgentInterface* InAgent)
{
	if(SetVoxelComplex(InIndex, InVoxelItem, true))
	{
		SpawnAuxiliary(InVoxelItem);
		GetVoxel(InIndex).OnGenerate(InAgent);
		return true;
	}
	return false;
}

bool AVoxelChunk::ReplaceVoxel(const FVoxelItem& InOldVoxelItem, const FVoxelItem& InNewVoxelItem, IVoxelAgentInterface* InAgent)
{
	if(SetVoxelComplex(InOldVoxelItem.Index, InNewVoxelItem, true))
	{
		GetVoxel(InOldVoxelItem.Index).OnReplace(InAgent, InOldVoxelItem);
		DestroyAuxiliary(InOldVoxelItem);
		SpawnAuxiliary(InNewVoxelItem);
		GetVoxel(InNewVoxelItem.Index).OnGenerate(InAgent);
		return true;
	}
	return false;
}

bool AVoxelChunk::DestroyVoxel(FIndex InIndex, IVoxelAgentInterface* InAgent)
{
	return DestroyVoxel(GetVoxelItem(InIndex));
}

bool AVoxelChunk::DestroyVoxel(const FVoxelItem& InVoxelItem, IVoxelAgentInterface* InAgent)
{
	if(SetVoxelComplex(InVoxelItem.Index, FVoxelItem::Empty, true))
	{
		GetVoxel(InVoxelItem.Index).OnDestroy(InAgent);
		DestroyAuxiliary(InVoxelItem);

		const auto& VoxelData = InVoxelItem.GetData<UVoxelData>();
		FVector range = VoxelData.GetCeilRange(InVoxelItem.Rotation, InVoxelItem.Scale);
		UAbilityModuleBPLibrary::SpawnPickUp(FAbilityItem(InVoxelItem.ID, 1), IndexToLocation(InVoxelItem.Index) + range * UVoxelModuleBPLibrary::GetWorldData().BlockSize * 0.5f, this);

		for(int32 x = 0; x < range.X; x++)
		{
			for(int32 y = 0; y < range.Y; y++)
			{
				FIndex tmpIndex = InVoxelItem.Index + FIndex(x, y, range.Z);
				const FVoxelItem& tmpVoxelItem = GetVoxelItem(tmpIndex);
				if(tmpVoxelItem.IsValid())
				{
					UVoxelData& tmpVoxelData = tmpVoxelItem.GetData<UVoxelData>();
					if(tmpVoxelData.Transparency == EVoxelTransparency::Transparent && tmpVoxelData.VoxelType != EVoxelType::Water)
					{
						DestroyVoxel(tmpVoxelItem);
					}
				}
			}
		}
		if(CheckNeighbors(InVoxelItem, EVoxelType::Water, true))
		{
			for(int32 x = 0; x < range.X; x++)
			{
				for(int32 y = 0; y < range.Y; y++)
				{
					for(int32 z = 0; z < range.Z; z++)
					{
						SetVoxelComplex(InVoxelItem.Index + FIndex(x, y, z), FVoxelItem(EVoxelType::Water), true);
					}
				}
			}
		}
		return true;
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
		if(VoxelMap.Contains(VoxelIndex) && VoxelMap[VoxelIndex].Auxiliary != Auxiliary)
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
		if(VoxelMap.Contains(VoxelIndex) && VoxelMap[VoxelIndex].Auxiliary == Auxiliary)
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

AVoxelAuxiliary* AVoxelChunk::SpawnAuxiliary(const FVoxelItem& InVoxelItem)
{
	const auto& VoxelData = InVoxelItem.GetData<UVoxelData>();
	if(!VoxelMap.Contains(InVoxelItem.Index) || VoxelMap[InVoxelItem.Index].Auxiliary || !VoxelData.AuxiliaryClass) return	nullptr;
	
	const FVector Location = IndexToLocation(InVoxelItem.Index, true) + VoxelData.GetCeilRange() * UVoxelModuleBPLibrary::GetWorldData().BlockSize * 0.5f;
	if(AVoxelAuxiliary* Auxiliary = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelAuxiliary>(nullptr, VoxelData.AuxiliaryClass))
	{
		Auxiliary->SetActorLocationAndRotation(Location, InVoxelItem.Rotation);
		Auxiliary->Initialize(this, InVoxelItem.Index);
		AddSceneActor(Auxiliary);
		return Auxiliary;
	}
	return nullptr;
}

void AVoxelChunk::DestroyAuxiliary(const FVoxelItem& InVoxelItem)
{
	if(!InVoxelItem.Auxiliary || !InVoxelItem.Auxiliary->IsValidLowLevel() || !VoxelMap.Contains(InVoxelItem.Index)) return;

	UObjectPoolModuleBPLibrary::DespawnObject(InVoxelItem.Auxiliary);
	VoxelMap[InVoxelItem.Index].Auxiliary = nullptr;
}

bool AVoxelChunk::IsEntity() const
{
	return !SolidMesh->IsEmpty() || !SemiMesh->IsEmpty();
}
