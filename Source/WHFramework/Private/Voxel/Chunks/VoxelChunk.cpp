// Fill out your copyright notice in the Description page of Project Settings.PickUp/


#include "Voxel/Chunks/VoxelChunk.h"

#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Character/Base/CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Scene/Object/PickUp/PickUp.h"
#include "Scene/Object/PickUp/PickUpEquip.h"
#include "Scene/Object/PickUp/PickUpProp.h"
#include "Scene/Object/PickUp/PickUpVoxel.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

// Sets default values
AVoxelChunk::AVoxelChunk()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	SolidMesh = nullptr;
	TransMesh = nullptr;

	Batch = 0;
	Index = FVector();
	bGenerated = false;
	VoxelMap = TMap<FIndex, FVoxelItem>();
	Neighbors = TArray<AVoxelChunk*>();
	Neighbors.SetNumZeroed(6);
	PickUps = TArray<APickUp*>();
}

// Called when the game starts or when spawned
void AVoxelChunk::BeginPlay()
{
	Super::BeginPlay();

}

void AVoxelChunk::OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherActor && OtherActor->IsA(ACharacterBase::StaticClass()))
	{
		if(IVoxelAgentInterface* voxelAgent = Cast<IVoxelAgentInterface>(OtherActor))
		{
			const FVoxelItem& voxelItem = GetVoxelItem(LocationToIndex(Hit.ImpactPoint - AVoxelModule::GetWorldData()->GetBlockSizedNormal(Hit.ImpactNormal)));
			if (voxelItem.IsValid())
			{
				if(UVoxel* voxel = voxelItem.GetVoxel())
				{
					voxel->OnTargetHit(voxelAgent, FVoxelHitResult(voxelItem, Hit.ImpactPoint, Hit.ImpactNormal));
					UVoxel::DespawnVoxel(voxel);
				}
			}
		}
	}
}

void AVoxelChunk::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// if(OtherComp && OtherComp->IsA(UAbilityCharacterPart::StaticClass()))
	// {
	// 	if(UAbilityCharacterPart* characterPart = Cast<UAbilityCharacterPart>(OtherComp))
	// 	{
	// 		if(ACharacterBase* character = characterPart->GetOwnerCharacter())
	// 		{
	// 			Debug(FString::Printf(TEXT("%s"), *SweepResult.ImpactPoint.ToString()));
	// 			Debug(FString::Printf(TEXT("%s"), *SweepResult.ImpactNormal.ToString()));
	// 			const FVector normal = FVector(FMath::Min((character->GetMoveDirection(false) * 2.5f).X, 1.f), FMath::Min((character->GetMoveDirection(false) * 2.5f).Y, 1.f), FMath::Min((character->GetMoveDirection(false) * 2.5f).Z, 1.f));
	// 			const FVector point = characterPart->GetComponentLocation() + characterPart->GetScaledCapsuleRadius() * normal + AVoxelModule::GetWorldData()->GetBlockSizedNormal(normal);
	// 			if(FVector::Distance(character->GetActorLocation(), point) > 100)
	// 			{
	// 				Debug(FString::Printf(TEXT("%f"), FVector::Distance(character->GetActorLocation(), point)));
	// 			}
	// 			UKismetSystemLibrary::DrawDebugBox(this, point, FVector(5), FColor::Green, FRotator::ZeroRotator, 100);
	// 			const FVoxelItem& voxelItem = GetVoxelItem(LocationToIndex(point));
	// 			const UVoxelAsset voxelData = voxelItem.GetData<UVoxelAsset>();
	// 			if(voxelItem.IsValid() && voxelData->VoxelType != characterPart->GetLastOverlapVoxel())
	// 			{
	// 				if(UVoxel* voxel = voxelItem.GetVoxel())
	// 				{
	// 					characterPart->SetLastVoxelType(voxelData->VoxelType);
	// 					voxel->OnTargetEnter(characterPart, FVoxelHitResult(voxelItem, point, normal));
	// 					UVoxel::DespawnVoxel(voxel);
	// 				}
	// 			}
	// 			character->OnBeginOverlapVoxel(characterPart, FVoxelHitResult(voxelItem, point, normal));
	// 		}
	// 	}
	// }
}

void AVoxelChunk::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// if(OtherComp && OtherComp->IsA(UAbilityCharacterPart::StaticClass()))
	// {
	// 	if(UAbilityCharacterPart* characterPart = Cast<UAbilityCharacterPart>(OtherComp))
	// 	{
	// 		if(ACharacterBase* character = characterPart->GetOwnerCharacter())
	// 		{
	// 			const FVector normal = FVector(FMath::Min((character->GetMoveDirection(false) * 2.5f).X, 1.f), FMath::Min((character->GetMoveDirection(false) * 2.5f).Y, 1.f), FMath::Min((character->GetMoveDirection(false) * 2.5f).Z, 1.f));
	// 			const FVector point = characterPart->GetComponentLocation() - characterPart->GetScaledCapsuleRadius() * normal - AVoxelModule::GetWorldData()->GetBlockSizedNormal(normal);
	// 			UKismetSystemLibrary::DrawDebugBox(this, point, FVector(5), FColor::Red, FRotator::ZeroRotator, 100);
	// 			const FVoxelItem& voxelItem = GetVoxelItem(LocationToIndex(point));
	// 			const UVoxelAsset voxelData = voxelItem.GetData<UVoxelAsset>();
	// 			if(voxelItem.IsValid() && voxelData->VoxelType == characterPart->GetLastOverlapVoxel())
	// 			{
	// 				const FVoxelItem& tmpVoxelItem = GetVoxelItem(LocationToIndex(characterPart->GetComponentLocation()));
	// 				const UVoxelAsset tmpVoxelData = tmpVoxelItem.GetData<UVoxelAsset>();
	// 				if(!tmpVoxelItem.IsValid() || tmpVoxelData->VoxelType != voxelData->VoxelType)
	// 				{
	// 					if(UVoxel* voxel = voxelItem.GetVoxel())
	// 					{
	// 						characterPart->SetLastVoxelType(EVoxelType::Unknown);
	// 						voxel->OnTargetExit(characterPart, FVoxelHitResult(voxelItem, point, normal));
	// 						UVoxel::DespawnVoxel(voxel);
	// 					}
	// 				}
	// 			}
	// 			character->OnEndOverlapVoxel(characterPart, FVoxelHitResult(voxelItem, point, normal));
	// 		}
	// 	}
	// }
}

// Called every frame
void AVoxelChunk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AVoxelChunk::OnSpawn_Implementation()
{
}

void AVoxelChunk::OnDespawn_Implementation()
{
	for (auto& iter : VoxelMap)
	{
		if(iter.Value.Auxiliary)
		{
			iter.Value.Auxiliary->Destroy();
		}
	}
	DestroyActors();

	BreakNeighbors();

	Index = FIndex::ZeroIndex;
	Batch = 0;
	bGenerated = false;
	
	VoxelMap.Empty();

	DestroySolidMesh();
	DestroySemiMesh();
	DestroyTransMesh();
}

UVoxelMeshComponent* AVoxelChunk::GetSolidMesh()
{
	if (!SolidMesh)
	{
		SolidMesh = NewObject<UVoxelMeshComponent>(this, TEXT("SolidMesh"));
		SolidMesh->RegisterComponent();
		SolidMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		SolidMesh->Initialize(EVoxelMeshType::Chunk, EVoxelTransparency::Solid);
		SolidMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);
	}
	return SolidMesh;
}

UVoxelMeshComponent* AVoxelChunk::GetSemiMesh()
{
	if (!SemiMesh)
	{
		SemiMesh = NewObject<UVoxelMeshComponent>(this, TEXT("SemiMesh"));
		SemiMesh->RegisterComponent();
		SemiMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		SemiMesh->Initialize(EVoxelMeshType::Chunk, EVoxelTransparency::SemiTransparent);
		SemiMesh->OnComponentHit.AddDynamic(this, &AVoxelChunk::OnCollision);
	}
	return SemiMesh;
}

UVoxelMeshComponent* AVoxelChunk::GetTransMesh()
{
	if (!TransMesh)
	{
		TransMesh = NewObject<UVoxelMeshComponent>(this, TEXT("TransMesh"));
		TransMesh->RegisterComponent();
		TransMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		TransMesh->Initialize(EVoxelMeshType::Chunk, EVoxelTransparency::Transparent);
		TransMesh->OnComponentBeginOverlap.AddDynamic(this, &AVoxelChunk::OnBeginOverlap);
		TransMesh->OnComponentEndOverlap.AddDynamic(this, &AVoxelChunk::OnEndOverlap);
	}
	return TransMesh;
}

void AVoxelChunk::DestroySolidMesh()
{
	if(SolidMesh)
	{
		SolidMesh->DestroyComponent();
		SolidMesh = nullptr;
	}
}

void AVoxelChunk::DestroySemiMesh()
{
	if(SemiMesh)
	{
		SemiMesh->DestroyComponent();
		SemiMesh = nullptr;
	}
}

void AVoxelChunk::DestroyTransMesh()
{
	if(TransMesh)
	{
		TransMesh->DestroyComponent();
		TransMesh = nullptr;
	}
}

void AVoxelChunk::Initialize(FIndex InIndex, int32 InBatch)
{
	Index = InIndex;
	Batch = InBatch;
	UpdateNeighbors();
}

void AVoxelChunk::LoadData(FSaveData* InVoxelChunkData)
{
}

FSaveData* AVoxelChunk::ToData(bool bSaved)
{
	return nullptr;
}

void AVoxelChunk::Generate(bool bPreview)
{
	if (VoxelMap.Num() > 0)
	{
		if(bGenerated)
		{
			GenerateMap();
		}
		if (SolidMesh)
		{
			if(!SolidMesh->IsEmpty())
			{
				SolidMesh->CreateMesh();
			}
			else
			{
				DestroySolidMesh();
			}
		}
		if (SemiMesh)
		{
			if(!SemiMesh->IsEmpty())
			{
				SemiMesh->CreateMesh();
			}
			else
			{
				DestroySemiMesh();
			}
		}
		if (TransMesh)
		{
			if(!TransMesh->IsEmpty())
			{
				TransMesh->CreateMesh();
			}
			else
			{
				DestroyTransMesh();
			}
		}
	}
	else
	{
		DestroySolidMesh();
		DestroySemiMesh();
		DestroyTransMesh();
	}

	if(!bGenerated)
	{
		OnGenerated(bPreview);
	}
}

void AVoxelChunk::BuildMap()
{
	for (int32 x = 0; x < AVoxelModule::GetWorldData()->ChunkSize; x++)
	{
		for (int32 y = 0; y < AVoxelModule::GetWorldData()->ChunkSize; y++)
		{
			for (int32 z = 0; z < AVoxelModule::GetWorldData()->ChunkSize; z++)
			{
				FIndex voxelIndex = FIndex(x, y, z);

				if (VoxelMap.Contains(voxelIndex)) continue;

				const EVoxelType voxelType = AVoxelModule::Get()->GetNoiseVoxelType(LocalIndexToWorld(voxelIndex));

				if (voxelType != EVoxelType::Empty)
				{
					switch (voxelType)
					{
						// grass
						case EVoxelType::Grass:
						{
							const float tmpNum = FMath::FRandRange(0.f, 1.f);
							// plant
							if (tmpNum < 0.2f)
							{
								SetVoxelComplex(FIndex(x, y, z + 1), FVoxelItem(FMath::FRandRange(0.f, 1.f) > 0.2f ? EVoxelType::Tall_Grass : (EVoxelType)FMath::RandRange((int32)EVoxelType::Flower_Allium, (int32)EVoxelType::Flower_Tulip_White)));
							}
							// tree
							else if (tmpNum < 0.21f)
							{
								if ((x > 2 && x <= AVoxelModule::GetWorldData()->ChunkSize - 2) && (y > 2 && y <= AVoxelModule::GetWorldData()->ChunkSize - 2))
								{
									const int32 treeHeight = FMath::RandRange(4.f, 5.f);
									const int32 leavesHeight = 2/*FMath::RandRange(2, 2)*/;
									const int32 leavesWidth = FMath::FRandRange(0.f, 1.f) < 0.5f ? 3 : 5;
									for (int32 trunkHeight = 0; trunkHeight < treeHeight; trunkHeight++)
									{
										SetVoxelComplex(FIndex(x, y, z + trunkHeight + 1), FVoxelItem(EVoxelType::Oak));
									}
									for (int32 offsetZ = treeHeight - leavesHeight; offsetZ < treeHeight + 1; offsetZ++)
									{
										for (int32 offsetX = -leavesWidth / 2; offsetX <= leavesWidth / 2; offsetX++)
										{
											for (int32 offsetY = -leavesWidth / 2; offsetY <= leavesWidth / 2; offsetY++)
											{
												const FVoxelItem& voxelItem = GetVoxelItem(x + offsetX, y + offsetY, z + offsetZ + 1);
												if (!voxelItem.IsValid() || voxelItem.GetData<UVoxelData>()->Transparency == EVoxelTransparency::Transparent)
												{
													SetVoxelComplex(FIndex(x + offsetX, y + offsetY, z + offsetZ + 1), FVoxelItem(EVoxelType::Oak_Leaves));
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
					SetVoxelSample(voxelIndex, FVoxelItem(voxelType));
				}
			}
		}
	}
}

void AVoxelChunk::GenerateMap()
{
	for (auto& iter : VoxelMap)
	{
		const FVoxelItem& voxelItem = iter.Value;
		if (voxelItem.IsValid())
		{
			switch (voxelItem.GetData<UVoxelData>()->Transparency)
			{
				case EVoxelTransparency::Solid:
				{
					if (GetSolidMesh())
					{
						GetSolidMesh()->BuildVoxel(voxelItem);
					}
					break;
				}
				case EVoxelTransparency::SemiTransparent:
				{
					if (GetSemiMesh())
					{
						GetSemiMesh()->BuildVoxel(voxelItem);
					}
					break;
				}
				case EVoxelTransparency::Transparent:
				{
					if (GetTransMesh())
					{
						GetTransMesh()->BuildVoxel(voxelItem);
					}
					break;
				}
				default: break;
			}
		}
	}
}

void AVoxelChunk::SpawnActors()
{
}

void AVoxelChunk::DestroyActors()
{
}

void AVoxelChunk::OnGenerated(bool bPreview)
{
	bGenerated = true;

	for (auto& iter : VoxelMap)
	{
		SpawnAuxiliary(iter.Value);
	}
	
	if(!bPreview)
	{
		SpawnActors();
	}
}

void AVoxelChunk::GenerateNeighbors(FIndex InIndex)
{
	GenerateNeighbors(InIndex.X, InIndex.Y, InIndex.Z);
}

void AVoxelChunk::GenerateNeighbors(int32 InX, int32 InY, int32 InZ)
{
	if (InX <= 0 && Neighbors[(int32)EDirection::Back] != nullptr) {
		Neighbors[(int32)EDirection::Back]->Generate();
	}
	else if (InX >= AVoxelModule::GetWorldData()->ChunkSize - 1 && Neighbors[(int32)EDirection::Forward] != nullptr) {
		Neighbors[(int32)EDirection::Forward]->Generate();
	}
	if (InY <= 0 && Neighbors[(int32)EDirection::Left] != nullptr) {
		Neighbors[(int32)EDirection::Left]->Generate();
	}
	else if (InY >= AVoxelModule::GetWorldData()->ChunkSize - 1 && Neighbors[(int32)EDirection::Right] != nullptr) {
		Neighbors[(int32)EDirection::Right]->Generate();
	}
	if (InZ <= 0 && Neighbors[(int32)EDirection::Down] != nullptr) {
		Neighbors[(int32)EDirection::Down]->Generate();
	}
	else if (InZ >= AVoxelModule::GetWorldData()->ChunkSize - 1 && Neighbors[(int32)EDirection::Up] != nullptr) {
		Neighbors[(int32)EDirection::Up]->Generate();
	}
}

void AVoxelChunk::UpdateNeighbors()
{
	for (int32 i = 0; i < 6; i++)
	{
		Neighbors[i] = AVoxelModule::Get()->FindChunk(Index + UVoxelModuleBPLibrary::DirectionToIndex((EDirection)i));
		if (Neighbors[i] != nullptr)
		{
			Neighbors[i]->Neighbors[(int32)UVoxelModuleBPLibrary::InvertDirection((EDirection)i)] = this;
		}
	}
}

void AVoxelChunk::BreakNeighbors()
{
	for (int32 i = 0; i < 6; i++)
	{
		if (Neighbors[i] != nullptr)
		{
			Neighbors[i]->Neighbors[(int32)UVoxelModuleBPLibrary::InvertDirection((EDirection)i)] = nullptr;
			Neighbors[i] = nullptr;
		}
	}
}

bool AVoxelChunk::IsOnTheChunk(FIndex InIndex) const
{
	return InIndex.X >= 0 && InIndex.X < AVoxelModule::GetWorldData()->ChunkSize &&
		InIndex.Y >= 0 && InIndex.Y < AVoxelModule::GetWorldData()->ChunkSize &&
		InIndex.Z >= 0 && InIndex.Z < AVoxelModule::GetWorldData()->ChunkSize;
}

bool AVoxelChunk::IsOnTheChunk(FVector InLocation) const
{
	return InLocation.X >= GetActorLocation().X && InLocation.X < GetActorLocation().X + AVoxelModule::GetWorldData()->GetChunkLength() &&
			InLocation.Y >= GetActorLocation().Y && InLocation.Y < GetActorLocation().Y + AVoxelModule::GetWorldData()->GetChunkLength() &&
			InLocation.Z >= GetActorLocation().Z && InLocation.Z < GetActorLocation().Z + AVoxelModule::GetWorldData()->GetChunkLength();
}

FIndex AVoxelChunk::LocationToIndex(FVector InLocation, bool bWorldSpace /*= true*/) const
{
	const FVector point = (!bWorldSpace ? InLocation : GetActorTransform().InverseTransformPosition(InLocation)) / AVoxelModule::GetWorldData()->BlockSize;

	FIndex index;
	index.X = FMath::FloorToInt(point.X);
	index.Y = FMath::FloorToInt(point.Y);
	index.Z = FMath::FloorToInt(point.Z);

	return index;
}

FVector AVoxelChunk::IndexToLocation(FIndex InIndex, bool bWorldSpace /*= true*/) const
{
	const FVector localPoint = InIndex.ToVector() * AVoxelModule::GetWorldData()->BlockSize;
	if (!bWorldSpace) return localPoint;
	return GetActorTransform().TransformPosition(localPoint);
}

FIndex AVoxelChunk::LocalIndexToWorld(FIndex InIndex) const
{
	return InIndex + Index * AVoxelModule::GetWorldData()->ChunkSize;
}

FIndex AVoxelChunk::WorldIndexToLocal(FIndex InIndex) const
{
	return InIndex - Index * AVoxelModule::GetWorldData()->ChunkSize;
}

UVoxel* AVoxelChunk::GetVoxel(FIndex InIndex)
{
	return GetVoxel(InIndex.X, InIndex.Y, InIndex.Z);
}

UVoxel* AVoxelChunk::GetVoxel(int32 InX, int32 InY, int32 InZ)
{
	return GetVoxelItem(InX, InY, InZ).GetVoxel();
}

FVoxelItem& AVoxelChunk::GetVoxelItem(FIndex InIndex)
{
	return GetVoxelItem(InIndex.X, InIndex.Y, InIndex.Z);
}

FVoxelItem& AVoxelChunk::GetVoxelItem(int32 InX, int32 InY, int32 InZ)
{
	if (InX < 0) {
		if (Neighbors[(int32)EDirection::Back] != nullptr)
			return Neighbors[(int32)EDirection::Back]->GetVoxelItem(InX + AVoxelModule::GetWorldData()->ChunkSize, InY, InZ);
		return FVoxelItem::UnknownVoxel;
	}
	else if (InX >= AVoxelModule::GetWorldData()->ChunkSize) {
		if (Neighbors[(int32)EDirection::Forward] != nullptr)
			return Neighbors[(int32)EDirection::Forward]->GetVoxelItem(InX - AVoxelModule::GetWorldData()->ChunkSize, InY, InZ);
		return FVoxelItem::UnknownVoxel;
	}
	else if (InY < 0) {
		if (Neighbors[(int32)EDirection::Left] != nullptr)
			return Neighbors[(int32)EDirection::Left]->GetVoxelItem(InX, InY + AVoxelModule::GetWorldData()->ChunkSize, InZ);
		return FVoxelItem::UnknownVoxel;
	}
	else if (InY >= AVoxelModule::GetWorldData()->ChunkSize) {
		if (Neighbors[(int32)EDirection::Right] != nullptr)
			return Neighbors[(int32)EDirection::Right]->GetVoxelItem(InX, InY - AVoxelModule::GetWorldData()->ChunkSize, InZ);
		return FVoxelItem::UnknownVoxel;
	}
	else if (InZ < 0) {
		if (Neighbors[(int32)EDirection::Down] != nullptr)
			return Neighbors[(int32)EDirection::Down]->GetVoxelItem(InX, InY, InZ + AVoxelModule::GetWorldData()->ChunkSize);
		else if (Index.Z > 0)
			return FVoxelItem::UnknownVoxel;
	}
	else if (InZ >= AVoxelModule::GetWorldData()->ChunkSize) {
		if (Neighbors[(int32)EDirection::Up] != nullptr)
			return Neighbors[(int32)EDirection::Up]->GetVoxelItem(InX, InY, InZ - AVoxelModule::GetWorldData()->ChunkSize);
		else if (Index.Z < AVoxelModule::GetWorldData()->ChunkHeightRange)
			return FVoxelItem::UnknownVoxel;
	}
	else if (VoxelMap.Contains(FIndex(InX, InY, InZ))) {
		return VoxelMap[FIndex(InX, InY, InZ)];
	}
	return FVoxelItem::EmptyVoxel;
}

bool AVoxelChunk::CheckVoxel(FIndex InIndex, FVector InRange/* = FVector::OneVector*/, bool bIgnoreTransparent /*= true*/)
{
	if (InRange != FVector::OneVector)
	{
		for (int32 x = 0; x < InRange.X; x++)
		{
			for (int32 y = 0; y < InRange.Y; y++)
			{
				for (int32 z = 0; z < InRange.Z; z++)
				{
					const FVoxelItem& voxelItem = GetVoxelItem(InIndex + FIndex(x, y, z));
					if (voxelItem.IsValid() && (!bIgnoreTransparent || voxelItem.GetData<UVoxelData>()->Transparency != EVoxelTransparency::Transparent))
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
		if (voxelItem.IsValid() && (!bIgnoreTransparent || voxelItem.GetData<UVoxelData>()->Transparency != EVoxelTransparency::Transparent))
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
	if(InDirection == EDirection::Down && LocalIndexToWorld(InVoxelItem.Index).Z == 0) return false;

	UVoxelData* voxelData = InVoxelItem.GetData<UVoxelData>();
	
	FVoxelItem& adjacentItem = GetVoxelItem(UVoxelModuleBPLibrary::GetAdjacentIndex(InVoxelItem.Index, InDirection, InVoxelItem.Rotation));
	UVoxelData* adjacentData = adjacentItem.GetData<UVoxelData>();

	if(adjacentItem.IsValid())
	{
		if(voxelData->Range * InVoxelItem.Scale == adjacentData->Range * adjacentItem.Scale)
		{
			switch (voxelData->Transparency)
			{
				case EVoxelTransparency::Solid:
				{
					switch (adjacentData->Transparency)
					{
						case EVoxelTransparency::Solid:
						{
							return false;
						}
						default: break;
					}
					break;
				}
				case EVoxelTransparency::SemiTransparent:
				{
					switch (adjacentData->Transparency)
					{
						case EVoxelTransparency::SemiTransparent:
						{
							if (voxelData->VoxelType == adjacentData->VoxelType)
							{
								switch (voxelData->VoxelType)
								{
									case EVoxelType::Oak_Leaves:
									case EVoxelType::Birch_Leaves:
									case EVoxelType::Ice:
									case EVoxelType::Glass:
									{
										return false;
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
					switch (adjacentData->Transparency)
					{
						case EVoxelTransparency::Solid:
						case EVoxelTransparency::SemiTransparent:
						{
							return false;
						}
						case EVoxelTransparency::Transparent:
						{
							if (voxelData->VoxelType == adjacentData->VoxelType)
							{
								switch (voxelData->VoxelType)
								{
									case EVoxelType::Water:
									{
										return false;
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
	else if(adjacentItem == FVoxelItem::UnknownVoxel)
	{
		return false;
	}
	return true;
}

bool AVoxelChunk::CheckNeighbors(const FVoxelItem& InVoxelItem, EVoxelType InVoxelType, bool bIgnoreBottom, int32 InDistance)
{
	const FVector range = InVoxelItem.GetData<UVoxelData>()->GetCeilRange(InVoxelItem.Rotation, InVoxelItem.Scale);
	for (int32 x = -InDistance; x < range.X + InDistance; x++)
	{
		for (int32 y = -InDistance; y < range.Y + InDistance; y++)
		{
			for (int32 z = bIgnoreBottom ? 0 : -InDistance; z < range.Z + InDistance; z++)
			{
				FVoxelItem& neighborItem = GetVoxelItem(Index + FIndex(x, y, z));
				if (neighborItem.IsValid())
				{
					if (neighborItem.GetData<UVoxelData>()->VoxelType == InVoxelType)
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool AVoxelChunk::SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerateMesh)
{
	bool RetValue = false;

	if (InVoxelItem.IsValid())
	{
		if (!CheckVoxel(InIndex))
		{
			FVoxelItem VoxelItem;
			VoxelItem.ID = InVoxelItem.ID;
			VoxelItem.Index = InIndex;
			VoxelItem.Owner = this;
			if (!VoxelMap.Contains(InIndex))
			{
				VoxelMap.Add(InIndex, VoxelItem);
			}
			else
			{
				//DestroyVoxel(InIndex);
				VoxelMap[InIndex] = VoxelItem;
			}
			RetValue = true;
		}
	}
	else if (VoxelMap.Contains(InIndex))
	{
		VoxelMap.Remove(InIndex);
		RetValue = true;
	}

	if (RetValue && bGenerateMesh)
	{
		Generate();
		GenerateNeighbors(InIndex);
	}

	return RetValue;
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
	if (InX < 0) {
		if (Neighbors[(int32)EDirection::Back] != nullptr)
			return Neighbors[(int32)EDirection::Back]->SetVoxelComplex(InX + AVoxelModule::GetWorldData()->ChunkSize, InY, InZ, InVoxelItem, bGenerateMesh);
	}
	else if (InX >= AVoxelModule::GetWorldData()->ChunkSize) {
		if (Neighbors[(int32)EDirection::Forward] != nullptr)
			return Neighbors[(int32)EDirection::Forward]->SetVoxelComplex(InX - AVoxelModule::GetWorldData()->ChunkSize, InY, InZ, InVoxelItem, bGenerateMesh);
	}
	else if (InY < 0) {
		if (Neighbors[(int32)EDirection::Left] != nullptr)
			return Neighbors[(int32)EDirection::Left]->SetVoxelComplex(InX, InY + AVoxelModule::GetWorldData()->ChunkSize, InZ, InVoxelItem, bGenerateMesh);
	}
	else if (InY >= AVoxelModule::GetWorldData()->ChunkSize) {
		if (Neighbors[(int32)EDirection::Right] != nullptr)
			return Neighbors[(int32)EDirection::Right]->SetVoxelComplex(InX, InY - AVoxelModule::GetWorldData()->ChunkSize, InZ, InVoxelItem, bGenerateMesh);
	}
	else if (InZ < 0) {
		if (Neighbors[(int32)EDirection::Down] != nullptr)
			return Neighbors[(int32)EDirection::Down]->SetVoxelComplex(InX, InY, InZ + AVoxelModule::GetWorldData()->ChunkSize, InVoxelItem, bGenerateMesh);
	}
	else if (InZ >= AVoxelModule::GetWorldData()->ChunkSize) {
		if (Neighbors[(int32)EDirection::Up] == nullptr)
			AVoxelModule::Get()->SpawnChunk(Index + UVoxelModuleBPLibrary::DirectionToIndex(EDirection::Up), !bGenerateMesh);
		if (Neighbors[(int32)EDirection::Up] != nullptr)
			return Neighbors[(int32)EDirection::Up]->SetVoxelComplex(InX, InY, InZ - AVoxelModule::GetWorldData()->ChunkSize, InVoxelItem, bGenerateMesh);
	}
	else {
		if (InVoxelItem.GetData<UVoxelData>()->IsComplex())
		{
			if (!InVoxelItem.IsEmpty())
			{
				const FVector range = InVoxelItem.GetData<UVoxelData>()->GetCeilRange(InVoxelItem.Rotation, InVoxelItem.Scale);
				if (!CheckVoxel(InX, InY, InZ, range))
				{
					for (int32 x = 0; x < range.X; x++)
					{
						for (int32 y = 0; y < range.Y; y++)
						{
							for (int32 z = 0; z < range.Z; z++)
							{
								FIndex index = FIndex(InX + x, InY + y, InZ + z);
								SetVoxelSample(index, InVoxelItem, bGenerateMesh);
							}
						}
					}
					return true;
				}
			}
			else
			{
				const FVoxelItem& voxelItem = GetVoxelItem(InX, InY, InZ);
				if(voxelItem.IsValid())
				{
					FVector range = voxelItem.GetData<UVoxelData>()->GetCeilRange(voxelItem.Rotation, voxelItem.Scale);
					for (int32 x = 0; x < range.X; x++)
					{
						for (int32 y = 0; y < range.Y; y++)
						{
							for (int32 z = 0; z < range.Z; z++)
							{
								FIndex index = voxelItem.Index + FIndex(InX, InY, InZ);
								if (VoxelMap.Contains(index))
								{
									VoxelMap.Remove(index);
								}
							}
						}
					}
					return true;
				}
			}
		}
		else
		{
			return SetVoxelSample(InX, InY, InZ, InVoxelItem, bGenerateMesh);
		}
	}
	return false;
}

bool AVoxelChunk::GenerateVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem)
{
	if(SetVoxelComplex(InIndex, InVoxelItem, true))
	{
		SpawnAuxiliary(GetVoxelItem(InIndex));
		UVoxelData* VoxelData = InVoxelItem.GetData<UVoxelData>();
		if(VoxelData->GenerateSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, VoxelData->GenerateSound, IndexToLocation(InIndex));
		}
		return true;
	}
	return false;
}

bool AVoxelChunk::DestroyVoxel(FIndex InIndex)
{
	return DestroyVoxel(GetVoxelItem(InIndex));
}

bool AVoxelChunk::DestroyVoxel(const FVoxelItem& InVoxelItem)
{
	if (SetVoxelComplex(InVoxelItem.Index, FVoxelItem::EmptyVoxel, true))
	{
		UVoxelData* voxelData = InVoxelItem.GetData<UVoxelData>();
		if(voxelData->GenerateSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, voxelData->GenerateSound, IndexToLocation(InVoxelItem.Index));
		}
		DestroyAuxiliary(InVoxelItem.Auxiliary);

		FVector range = voxelData->GetCeilRange(InVoxelItem.Rotation, InVoxelItem.Scale);
		SpawnPickUp(FAbilityItem(InVoxelItem.ID, 1), IndexToLocation(InVoxelItem.Index) + range * AVoxelModule::GetWorldData()->BlockSize * 0.5f);

		for (int32 x = 0; x < range.X; x++)
		{
			for (int32 y = 0; y < range.Y; y++)
			{
				FIndex tmpIndex = InVoxelItem.Index + FIndex(x, y, range.Z);
				const FVoxelItem& tmpVoxelItem = GetVoxelItem(tmpIndex);
				if (tmpVoxelItem.IsValid())
				{
					UVoxelData* tmpVoxelData = tmpVoxelItem.GetData<UVoxelData>();
					if(tmpVoxelData->Transparency == EVoxelTransparency::Transparent && tmpVoxelData->VoxelType != EVoxelType::Water)
					{
						DestroyVoxel(tmpVoxelItem);
					}
				}
			}
		}
		if (CheckNeighbors(InVoxelItem, EVoxelType::Water, true))
		{
			for (int32 x = 0; x < range.X; x++)
			{
				for (int32 y = 0; y < range.Y; y++)
				{
					for (int32 z = 0; z < range.Z; z++)
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

bool AVoxelChunk::ReplaceVoxel(const FVoxelItem& InOldVoxelItem, const FVoxelItem& InNewVoxelItem)
{
	if (SetVoxelComplex(InOldVoxelItem.Index, InNewVoxelItem, true))
	{
		UVoxelData* VoxelData = InNewVoxelItem.GetData<UVoxelData>();
		SpawnAuxiliary(GetVoxelItem(InOldVoxelItem.Index));
		if(VoxelData->GenerateSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, VoxelData->GenerateSound, IndexToLocation(InNewVoxelItem.Index));
		}
		return true;
	}
	return false;
}

AVoxelAuxiliary* AVoxelChunk::SpawnAuxiliary(FVoxelItem& InVoxelItem)
{
	UVoxelData* VoxelData = InVoxelItem.GetData<UVoxelData>();
	if (VoxelMap.Contains(InVoxelItem.Index) && VoxelData->AuxiliaryClass)
	{
		FActorSpawnParameters spawnParams = FActorSpawnParameters();
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Location = IndexToLocation(InVoxelItem.Index, true) + VoxelData->GetCeilRange() * AVoxelModule::GetWorldData()->BlockSize * 0.5f;
		if (AVoxelAuxiliary* Auxiliary = GetWorld()->SpawnActor<AVoxelAuxiliary>(VoxelData->AuxiliaryClass, Location, InVoxelItem.Rotation, spawnParams))
		{
			InVoxelItem.Auxiliary = Auxiliary;
			Auxiliary->Initialize(this, InVoxelItem.Index);
			Auxiliary->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			//SetVoxelSample(InVoxelItem.Index, InVoxelItem, false, false);
			return Auxiliary;
		}
	}
	return nullptr;
}

void AVoxelChunk::DestroyAuxiliary(AVoxelAuxiliary* InAuxiliary)
{
	if(!InAuxiliary || !InAuxiliary->IsValidLowLevel()) return;

	if(VoxelMap.Contains(InAuxiliary->GetVoxelItem().Index))
	{
		InAuxiliary->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		InAuxiliary->Destroy();
		InAuxiliary->GetVoxelItem().Auxiliary = nullptr;
	}
}

void AVoxelChunk::SpawnSceneObject(ISceneObjectInterface* InSceneObject)
{
	
}

void AVoxelChunk::AddSceneObject(ISceneObjectInterface* InSceneObject)
{
	if(APickUp* PickUp = Cast<APickUp>(InSceneObject))
	{
		AttachPickUp(PickUp);
	}
}

void AVoxelChunk::RemoveSceneObject(ISceneObjectInterface* InSceneObject)
{
	if(APickUp* PickUp = Cast<APickUp>(InSceneObject))
	{
		DetachPickUp(PickUp);
	}
}

void AVoxelChunk::DestroySceneObject(ISceneObjectInterface* InSceneObject)
{
	if(APickUp* PickUp = Cast<APickUp>(InSceneObject))
	{
		DestroyPickUp(PickUp);
	}
}

APickUp* AVoxelChunk::SpawnPickUp(FAbilityItem InItem, FVector InLocation)
{
	if (InItem == FAbilityItem::Empty) return nullptr;

	APickUp* pickUpItem = nullptr;
	
	FActorSpawnParameters spawnParams = FActorSpawnParameters();
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	if(InItem.GetData()->EqualType(EAbilityItemType::Voxel))
	{
		pickUpItem = GetWorld()->SpawnActor<APickUpVoxel>(InLocation, FRotator::ZeroRotator, spawnParams);
	}
	else if(InItem.GetData()->EqualType(EAbilityItemType::Equip))
	{
		pickUpItem = GetWorld()->SpawnActor<APickUpEquip>(InLocation, FRotator::ZeroRotator, spawnParams);
	}
	else if(InItem.GetData()->EqualType(EAbilityItemType::Prop))
	{
		pickUpItem = GetWorld()->SpawnActor<APickUpProp>(InLocation, FRotator::ZeroRotator, spawnParams);
	}
	
	if (pickUpItem != nullptr)
	{
		AttachPickUp(pickUpItem);
		pickUpItem->Initialize(InItem);
	}
	return pickUpItem;
}

APickUp* AVoxelChunk::SpawnPickUp(FPickUpSaveData InPickUpData)
{
	return SpawnPickUp(InPickUpData.Item, InPickUpData.Location);
}

void AVoxelChunk::AttachPickUp(APickUp* InPickUp)
{
	if(!InPickUp || !InPickUp->IsValidLowLevel() || PickUps.Contains(InPickUp)) return;

	InPickUp->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	InPickUp->SetContainer(this);
	PickUps.Add(InPickUp);
}

void AVoxelChunk::DetachPickUp(APickUp* InPickUp)
{
	if(!InPickUp || !InPickUp->IsValidLowLevel() || !PickUps.Contains(InPickUp)) return;

	InPickUp->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	InPickUp->SetContainer(nullptr);
	PickUps.Remove(InPickUp);
}

void AVoxelChunk::DestroyPickUp(APickUp* InPickUp)
{
	if(!InPickUp || !InPickUp->IsValidLowLevel()) return;

	if (PickUps.Contains(InPickUp))
	{
		PickUps.Remove(InPickUp);
		InPickUp->Destroy();
	}
}
