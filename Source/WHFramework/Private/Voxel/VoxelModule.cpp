// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModule.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Character/Base/CharacterBase.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Voxel/EventHandle_ChangeVoxelWorldState.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Global/GlobalBPLibrary.h"
#include "Main/MainModule.h"
#include "Math/MathBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Scene/SceneModule.h"
#include "Scene/Components/WorldTimerComponent.h"
#include "ReferencePool/ReferencePoolModuleBPLibrary.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelDoor.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/VoxelTorch.h"
#include "Voxel/Voxels/VoxelWater.h"
#include "Voxel/Voxels/Entity/VoxelEntity.h"

// Sets default values
AVoxelModule::AVoxelModule()
{
	ModuleName = FName("VoxelModule");

	VoxelsCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("VoxelsCapture"));
	VoxelsCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	VoxelsCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	VoxelsCapture->SetupAttachment(RootComponent);
	VoxelsCapture->SetRelativeLocationAndRotation(FVector(0, 0, -500), FRotator(90, 0, 0));

	WorldMode = EVoxelWorldMode::None;
	WorldState = EVoxelWorldState::None;

	ChunkSpawnClass = AVoxelChunk::StaticClass();
	
	ChunkSpawnRange = 7;
	ChunkBasicSpawnRange = 1;
	ChunkSpawnDistance = 3;

	ChunkSpawnSpeed = 1000;
	ChunkDestroyDistance = 1;
	ChunkDestroySpeed = 10;
	ChunkMapLoadSpeed = 100;
	ChunkMapBuildSpeed = 100;
	ChunkMeshBuildSpeed = 100;
	ChunkGenerateSpeed = 1;

	ChunkSpawnBatch = 0;
	LastGenerateIndex = FIndex(-1, -1, -1);
	LastStayChunkIndex = FIndex::ZeroIndex;
	ChunkMap = TMap<FIndex, AVoxelChunk*>();
	ChunkSpawnQueue = TArray<FIndex>();
	ChunkMapLoadQueue = TArray<FIndex>();
	ChunkMapBuildQueue = TArray<FIndex>();
	ChunkMapBuildQueue1 = TArray<FIndex>();
	ChunkMeshBuildQueue = TArray<FIndex>();
	ChunkGenerateQueue = TArray<FIndex>();
	ChunkDestroyQueue = TArray<FIndex>();

	WorldBasicData = FVoxelWorldBasicSaveData();
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SolidMatFinder(TEXT("/WHFramework/Voxel/Materials/M_Voxels_Solid.M_Voxels_Solid"));
	if(SolidMatFinder.Succeeded())
	{
		WorldBasicData.ChunkMaterials.Add(FVoxelChunkMaterial(SolidMatFinder.Object, FVector2D(0.0625f, 0.5f)));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SemiTransparentMatFinder(TEXT("/WHFramework/Voxel/Materials/M_Voxels_SemiTransparent.M_Voxels_SemiTransparent"));
	if(SemiTransparentMatFinder.Succeeded())
	{
		WorldBasicData.ChunkMaterials.Add(FVoxelChunkMaterial(SemiTransparentMatFinder.Object, FVector2D(0.0625f, 0.5f)));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TransparentMatFinder(TEXT("/WHFramework/Voxel/Materials/M_Voxels_Transparent.M_Voxels_Transparent"));
	if(TransparentMatFinder.Succeeded())
	{
		WorldBasicData.ChunkMaterials.Add(FVoxelChunkMaterial(TransparentMatFinder.Object, FVector2D(0.0625f, 0.5f)));
	}
}

#if WITH_EDITOR
void AVoxelModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AVoxelModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AVoxelModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AVoxelModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	UAssetModuleBPLibrary::LoadPrimaryAssets<UVoxelData>(UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Voxel));

	UReferencePoolModuleBPLibrary::CreateReference<UVoxel>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelEmpty>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelUnknown>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelDoor>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelPlant>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelTorch>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelWater>();

	if(WorldMode != EVoxelWorldMode::None)
	{
		static FVoxelWorldSaveData SaveData = FVoxelWorldSaveData(WorldBasicData);
		LoadSaveData(&SaveData, WorldMode == EVoxelWorldMode::Normal);
	}
}

void AVoxelModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	GenerateWorld();
}

void AVoxelModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AVoxelModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AVoxelModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	StopChunkTasks(ChunkMapLoadTasks);
	StopChunkTasks(ChunkMapBuildTasks);
	StopChunkTasks(ChunkMeshBuildTasks);
}

float AVoxelModule::GetWorldLength() const
{
	return WorldData->ChunkSize * ChunkSpawnRange * 2;
}

bool AVoxelModule::IsBasicGenerated() const
{
	const int32 basicNum = FMath::Square(ChunkBasicSpawnRange * 2) * WorldData->ChunkHeightRange;
	return GetChunkNum(true) >= basicNum;
}

void AVoxelModule::SetWorldState(EVoxelWorldState InWorldState)
{
	if(WorldState != InWorldState)
	{
		WorldState = InWorldState;
		OnWorldStateChanged();
	}
}

void AVoxelModule::OnWorldStateChanged()
{
	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_ChangeVoxelWorldState::StaticClass(), EEventNetType::Single, this, {FParameter::MakePointer(&WorldState)});
}

FVoxelWorldSaveData& AVoxelModule::GetWorldData() const
{
	return WorldData ? *WorldData : FVoxelWorldSaveData::Empty;
}

void AVoxelModule::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	WorldData = InSaveData->Cast<FVoxelWorldSaveData>();
	if(WorldData->WorldSeed == 0)
	{
		WorldData->WorldSeed = FMath::Rand();
	}
	WorldData->RandomStream = FRandomStream(WorldData->WorldSeed);
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		if(UWorldTimerComponent* WorldTimer = SceneModule->GetWorldTimer())
		{
			WorldTimer->SetTimeSeconds(WorldData->TimeSeconds);
		}
	}

	if(!bForceMode)
	{
		WorldMode = EVoxelWorldMode::Normal;
		for(auto Iter : ChunkMap)
		{
			if(Iter.Value && Iter.Value->IsGenerated())
			{
				Iter.Value->SpawnActors();
			}
		}
	}
}

FSaveData* AVoxelModule::ToData()
{
	for(auto Iter : ChunkMap)
	{
		if(Iter.Value)
		{
			WorldData->SetChunkData(Iter.Key, Iter.Value->ToSaveData<FVoxelChunkSaveData>());
		}
	}
	return WorldData;
}

void AVoxelModule::UnloadData(bool bForceMode)
{
	if(bForceMode)
	{
		SetWorldState(EVoxelWorldState::None);
		
		StopChunkTasks(ChunkMapLoadTasks);
		StopChunkTasks(ChunkMapBuildTasks);
		StopChunkTasks(ChunkMeshBuildTasks);

		for(auto iter : ChunkMap)
		{
			if(iter.Value)
			{
				UObjectPoolModuleBPLibrary::DespawnObject(iter.Value);
			}
		}
		ChunkMap.Empty();

		ChunkSpawnBatch = 0;
		LastGenerateIndex = FIndex(-1, -1, -1);
		LastStayChunkIndex = FIndex::ZeroIndex;

		ChunkSpawnQueue.Empty();
		ChunkMapLoadQueue.Empty();
		ChunkMapBuildQueue.Empty();
		ChunkMapBuildQueue1.Empty();
		ChunkMeshBuildQueue.Empty();
		ChunkGenerateQueue.Empty();
		ChunkDestroyQueue.Empty();

		WorldData = nullptr;
	}
	else
	{
		WorldMode = EVoxelWorldMode::Preview;
		for(auto Iter : ChunkMap)
		{
			if(Iter.Value && Iter.Value->IsGenerated())
			{
				Iter.Value->DestroyActors();
			}
		}
	}
}

void AVoxelModule::GenerateWorld()
{
	const IVoxelAgentInterface* VoxelAgent = UGlobalBPLibrary::GetPlayerPawn<IVoxelAgentInterface>();

	if(!WorldData || !VoxelAgent || VoxelAgent->GetWorldLocation().Z < 0.f) return;
	
	const FIndex ChunkIndex = UVoxelModuleBPLibrary::LocationToChunkIndex(VoxelAgent->GetWorldLocation(), true);

	// Check chunk spawn queue
	if(LastGenerateIndex == FIndex(-1, -1, -1) || FIndex::Distance(ChunkIndex, LastGenerateIndex, true) >= ChunkSpawnDistance)
	{
		if(ChunkSpawnQueue.Num() > 0) return;

		for(int32 x = ChunkIndex.X - ChunkSpawnRange; x < ChunkIndex.X + ChunkSpawnRange; x++)
		{
			for(int32 y = ChunkIndex.Y - ChunkSpawnRange; y < ChunkIndex.Y + ChunkSpawnRange; y++)
			{
				for(int32 z = 0; z < WorldData->ChunkHeightRange; z++)
				{
					if(FIndex::Distance(FIndex(x, y, z), ChunkIndex, true) < ChunkSpawnRange + ChunkDestroyDistance)
					{
						AddToSpawnQueue(FIndex(x, y, z));
					}
				}
			}
		}
		LastGenerateIndex = ChunkIndex;
		ChunkSpawnBatch++;
	}
	// Check chunk destroy queue
	if(ChunkIndex != LastStayChunkIndex)
	{
		LastStayChunkIndex = ChunkIndex;
		for(auto iter = ChunkMap.CreateConstIterator(); iter; ++iter)
		{
			const FIndex index = iter->Key;
			if(FIndex::Distance(ChunkIndex, index, true) >= ChunkSpawnRange + ChunkDestroyDistance)
			{
				AddToDestroyQueue(index);
			}
			else
			{
				RemoveFromDestroyQueue(index);
			}
		}
	}
	// Destroy chunk
	if(UpdateChunkQueue(ChunkDestroyQueue, ChunkDestroySpeed, [this](FIndex Index){ DestroyChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Destroying);
	}
	// Spawn chunk
	else if(UpdateChunkQueue(ChunkSpawnQueue, ChunkSpawnSpeed, [this](FIndex Index){ SpawnChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Spawning);
	}
	// Load chunk map
	else if(UpdateChunkTasks(ChunkMapLoadQueue, ChunkMapLoadTasks, ChunkMapLoadSpeed))
	{
		SetWorldState(EVoxelWorldState::LoadingMap);
	}
	// Build chunk map state 0
	else if(UpdateChunkTasks(ChunkMapBuildQueue, ChunkMapBuildTasks, ChunkMapBuildSpeed))
	{
		SetWorldState(EVoxelWorldState::BuildingMap);
	}
	// Build chunk map state 1
	else if(UpdateChunkQueue(ChunkMapBuildQueue1, ChunkMapBuildSpeed, [this](FIndex Index, int32 InStage){ BuildChunkMap(Index, InStage); }, 1))
	{
		SetWorldState(EVoxelWorldState::BuildingMap);
	}
	// Build chunk mesh
	else if(UpdateChunkTasks(ChunkMeshBuildQueue, ChunkMeshBuildTasks, ChunkMeshBuildSpeed))
	{
		SetWorldState(EVoxelWorldState::BuildingMesh);
	}
	// Generate chunk
	else if(UpdateChunkQueue(ChunkGenerateQueue, ChunkGenerateSpeed, [this](FIndex Index){ GenerateChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Generating);
	}
	else
	{
		SetWorldState(EVoxelWorldState::None);
	}
}

void AVoxelModule::GenerateVoxels()
{
	auto voxelDatas = UAssetModuleBPLibrary::LoadPrimaryAssets<UVoxelData>(UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Voxel));
	if(voxelDatas.Num() >0)
	{
		int32 tmpNum = 8;
		int32 tmpIndex = 0;
		VoxelsCapture->OrthoWidth = tmpNum * WorldData->BlockSize * 0.5f;
		for (float x = -(tmpNum - 1) * 0.5f; x <= (tmpNum - 1) * 0.5f; x++)
		{
			for (float y = -(tmpNum - 1) * 0.5f; y <= (tmpNum - 1) * 0.5f; y++)
			{
				if (tmpIndex < voxelDatas.Num())
				{
					if (AVoxelEntity* voxelEntity = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelEntity>())
					{
						voxelEntity->Initialize(voxelDatas[tmpIndex]->GetPrimaryAssetId());
						voxelEntity->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
						voxelEntity->SetActorLocation(FVector(x * WorldData->BlockSize * 0.5f, y * WorldData->BlockSize * 0.5f, 0));
						voxelEntity->SetActorRotation(FRotator(-70, 0, -180));
						voxelEntity->GetMeshComponent()->SetRelativeRotation(FRotator(0, 45, 0));
						VoxelsCapture->ShowOnlyActors.Add(voxelEntity);
					}
					tmpIndex++;
				}
			}
		}
	}
}

void AVoxelModule::LoadChunkMap(FIndex InIndex)
{
	if (!ChunkMap.Contains(InIndex) || !RemoveFromMapLoadQueue(InIndex)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->LoadSaveData(WorldData->GetChunkData(InIndex));

	AddToMeshBuildQueue(InIndex);
}

void AVoxelModule::BuildChunkMap(FIndex InIndex, int32 InStage)
{
	if (!ChunkMap.Contains(InIndex) || !RemoveFromMapBuildQueue(InIndex, InStage)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->BuildMap(InStage);

	if(InStage < 1)
	{
		AddToMapBuildQueue(InIndex, InStage + 1);
	}
	else
	{
		AddToMeshBuildQueue(InIndex);
	}
}

void AVoxelModule::BuildChunkMesh(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex) || !RemoveFromMeshBuildQueue(InIndex)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->BuildMesh();

	AddToGenerateQueue(InIndex);
}

void AVoxelModule::GenerateChunk(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex) || !RemoveFromGenerateQueue(InIndex)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->Generate(WorldMode == EVoxelWorldMode::Normal);
}

void AVoxelModule::DestroyChunk(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex) || !RemoveFromDestroyQueue(InIndex)) return;
	
	RemoveFromMapLoadQueue(InIndex);
	RemoveFromMapBuildQueue(InIndex, 0);
	RemoveFromMapBuildQueue(InIndex, 1);
	RemoveFromMeshBuildQueue(InIndex);
	RemoveFromGenerateQueue(InIndex);

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);
	
	WorldData->SetChunkData(InIndex, Chunk->ToSaveData<FVoxelChunkSaveData>());
	UObjectPoolModuleBPLibrary::DespawnObject(Chunk);

	ChunkMap.Remove(InIndex);
}

bool AVoxelModule::AddToSpawnQueue(FIndex InIndex)
{
	if(ChunkMap.Contains(InIndex)) return false;

	ChunkSpawnQueue.AddUnique(InIndex);
	return true;
}

bool AVoxelModule::RemoveFromSpawnQueue(FIndex InIndex)
{
	if(ChunkSpawnQueue.Contains(InIndex))
	{
		ChunkSpawnQueue.Remove(InIndex);
		return true;
	}
	return false;
}

bool AVoxelModule::AddToMapLoadQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return false;
	
	ChunkMapLoadQueue.AddUnique(InIndex);
	return true;
}

bool AVoxelModule::RemoveFromMapLoadQueue(FIndex InIndex)
{
	if(ChunkMapLoadQueue.Contains(InIndex))
	{
		ChunkMapLoadQueue.Remove(InIndex);
		return true;
	}
	return false;
}

bool AVoxelModule::AddToMapBuildQueue(FIndex InIndex, int32 InStage)
{
	if(!ChunkMap.Contains(InIndex)) return false;

	switch (InStage)
	{
		case 0:
		{
			ChunkMapBuildQueue.AddUnique(InIndex);
			break;
		}
		case 1:
		{
			ChunkMapBuildQueue1.AddUnique(InIndex);
			break;
		}
		default: break;
	}
	return true;
}

bool AVoxelModule::RemoveFromMapBuildQueue(FIndex InIndex, int32 InStage)
{
	switch (InStage)
	{
		case 0:
		{
			if(ChunkMapBuildQueue.Contains(InIndex))
			{
				ChunkMapBuildQueue.Remove(InIndex);
				return true;
			}
			break;
		}
		case 1:
		{
			if(ChunkMapBuildQueue1.Contains(InIndex))
			{
				ChunkMapBuildQueue1.Remove(InIndex);
				return true;
			}
			break;
		}
		default: break;
	}
	return false;
}

bool AVoxelModule::AddToMeshBuildQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return false;
	
	ChunkMeshBuildQueue.AddUnique(InIndex);
	return true;
}

bool AVoxelModule::RemoveFromMeshBuildQueue(FIndex InIndex)
{
	if(ChunkMeshBuildQueue.Contains(InIndex))
	{
		ChunkMeshBuildQueue.Remove(InIndex);
		return true;
	}
	return false;
}

bool AVoxelModule::AddToGenerateQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return false;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	ChunkGenerateQueue.AddUnique(InIndex);
	for(auto Iter : Chunk->GetNeighbors())
	{
		if(Iter.Value && Iter.Value->GetBatch() != Chunk->GetBatch())
		{
			ChunkGenerateQueue.AddUnique(Iter.Value->GetIndex());
		}
	}
	ChunkGenerateQueue.Sort([this](const FIndex& A, const FIndex& B){
		const float lengthA = FIndex::Distance(LastGenerateIndex, A, true);
		const float lengthB = FIndex::Distance(LastGenerateIndex, B, true);
		if(lengthA == lengthB) return A.Z < B.Z;
		return lengthA < lengthB;
	});
	return true;
}

bool AVoxelModule::RemoveFromGenerateQueue(FIndex InIndex)
{
	if(ChunkGenerateQueue.Contains(InIndex))
	{
		ChunkGenerateQueue.Remove(InIndex);
		return true;
	}
	return false;
}

bool AVoxelModule::AddToDestroyQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return false;
	
	ChunkDestroyQueue.AddUnique(InIndex);
	ChunkDestroyQueue.Sort([this](const FIndex& A, const FIndex& B){
		const float lengthA = FIndex::Distance(LastGenerateIndex, A, true);
		const float lengthB = FIndex::Distance(LastGenerateIndex, B, true);
		if(lengthA == lengthB) return A.Z > B.Z;
		return lengthA > lengthB;
	});
	return true;
}

bool AVoxelModule::RemoveFromDestroyQueue(FIndex InIndex)
{
	if(ChunkDestroyQueue.Contains(InIndex))
	{
		ChunkDestroyQueue.Remove(InIndex);
		return true;
	}
	return false;
}

AVoxelChunk* AVoxelModule::SpawnChunk(FIndex InIndex, bool bAddToQueue)
{
	if(ChunkMap.Contains(InIndex)) return ChunkMap[InIndex];

	RemoveFromSpawnQueue(InIndex);
	
	AVoxelChunk* Chunk = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelChunk>(nullptr, ChunkSpawnClass);
	if(Chunk)
	{
		Chunk->SetActorLocationAndRotation(InIndex.ToVector() * WorldData->GetChunkLength(), FRotator::ZeroRotator);

		ChunkMap.Add(InIndex, Chunk);

		Chunk->Initialize(this, InIndex, ChunkSpawnBatch);
		Chunk->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

		if(bAddToQueue)
		{
			if (WorldData->IsExistChunkData(InIndex))
			{
				AddToMapLoadQueue(InIndex);
			}
			else
			{
				AddToMapBuildQueue(InIndex, 0);
			}
		}
	}
	return Chunk;
}

AVoxelChunk* AVoxelModule::FindChunkByIndex(FIndex InIndex)
{
	if(ChunkMap.Contains(InIndex))
	{
		return ChunkMap[InIndex];
	}
	return nullptr;
}

AVoxelChunk* AVoxelModule::FindChunkByLocation(FVector InLocation)
{
	return FindChunkByIndex(UVoxelModuleBPLibrary::LocationToChunkIndex(InLocation));
}

EVoxelType AVoxelModule::GetNoiseVoxelType(FIndex InIndex)
{
	const FVector offsetIndex = FVector(InIndex.X + WorldData->WorldSeed, InIndex.Y + WorldData->WorldSeed, InIndex.Z);

	const int32 plainHeight = GetNoiseTerrainHeight(offsetIndex, WorldData->TerrainPlainScale);
	const int32 mountainHeight = GetNoiseTerrainHeight(offsetIndex, WorldData->TerrainMountainScale);

	const int32 baseHeight = FMath::Clamp(FMath::Max(plainHeight, mountainHeight) + int32(WorldData->GetWorldHeight() * WorldData->TerrainBaseHeight), 0, WorldData->GetWorldHeight() - 1);

	const int32 stoneHeight = FMath::Clamp(GetNoiseTerrainHeight(offsetIndex, WorldData->TerrainStoneVoxelScale), 0, WorldData->GetWorldHeight() - 1);
	const int32 sandHeight = FMath::Clamp(GetNoiseTerrainHeight(offsetIndex, WorldData->TerrainSandVoxelScale), 0, WorldData->GetWorldHeight() - 1);

	const int32 waterHeight = FMath::Clamp(int32(WorldData->GetWorldHeight() * WorldData->TerrainWaterVoxelHeight), 0, WorldData->GetWorldHeight() - 1);
	const int32 bedrockHeight = FMath::Clamp(int32(WorldData->GetWorldHeight() * WorldData->TerrainBedrockVoxelHeight), 0, WorldData->GetWorldHeight() - 1);

	if(InIndex.Z < baseHeight)
	{
		if(InIndex.Z <= bedrockHeight)
		{
			return EVoxelType::Bedrock; //Bedrock
		}
		else if(InIndex.Z <= stoneHeight)
		{
			return EVoxelType::Stone; //Stone
		}
		return EVoxelType::Dirt; //Dirt
	}
	else if(InIndex.Z <= sandHeight)
	{
		return EVoxelType::Sand; //Sand
	}
	else if(InIndex.Z <= waterHeight)
	{
		return EVoxelType::Water; //Water
	}
	else if(InIndex.Z == baseHeight)
	{
		return EVoxelType::Grass; //Grass
	}
	return EVoxelType::Empty; //Empty
}

UVoxelData& AVoxelModule::GetNoiseVoxelData(FIndex InIndex)
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(FPrimaryAssetId::FromString(*FString::Printf(TEXT("Voxel_%d"), (int32)GetNoiseVoxelType(InIndex))));
}

int32 AVoxelModule::GetNoiseTerrainHeight(FVector InOffset, FVector InScale)
{
	return (FMath::PerlinNoise2D(FVector2D(InOffset.X * InScale.X, InOffset.Y * InScale.Y)) + 1) * WorldData->GetWorldHeight() * InScale.Z;
}

bool AVoxelModule::ChunkTraceSingle(AVoxelChunk* InChunk, float InRadius, float InHalfHeight, FHitResult& OutHitResult)
{
	return false;
}

bool AVoxelModule::ChunkTraceSingle(FVector RayStart, FVector RayEnd, float InRadius, float InHalfHeight, FHitResult& OutHitResult)
{
	return false;
}

bool AVoxelModule::VoxelTraceSingle(const FVoxelItem& InVoxelItem, FVector InPoint, FHitResult& OutHitResult)
{
	return false;
}

int32 AVoxelModule::GetChunkNum(bool bNeedGenerated /*= false*/) const
{
	int32 num = 0;
	for(auto iter = ChunkMap.CreateConstIterator(); iter; ++iter)
	{
		if(AVoxelChunk* chunk = iter->Value)
		{
			if(!bNeedGenerated || chunk->IsGenerated())
			{
				num++;
			}
		}
	}
	return num;
}
