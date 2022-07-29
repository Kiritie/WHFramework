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
	ChunkMapLoadSpeed = 10;
	ChunkMapBuildSpeed = 100;
	ChunkMapGenerateSpeed = 10;
	ChunkGenerateSpeed = 1;

	ChunkSpawnBatch = 0;
	LastGenerateIndex = FIndex(-1, -1, -1);
	LastStayChunkIndex = FIndex::ZeroIndex;
	ChunkMap = TMap<FIndex, AVoxelChunk*>();
	ChunkSpawnQueue = TArray<FIndex>();
	ChunkMapLoadQueue = TArray<FIndex>();
	ChunkMapBuildQueue = TArray<FIndex>();
	ChunkMapBuildQueue1 = TArray<FIndex>();
	ChunkMapGenerateQueue = TArray<FIndex>();
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

	UObjectPoolModuleBPLibrary::DespawnObject(UObjectPoolModuleBPLibrary::SpawnObject<AVoxelChunk>(nullptr, ChunkSpawnClass));
}

void AVoxelModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	UReferencePoolModuleBPLibrary::CreateReference<UVoxel>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelEmpty>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelUnknown>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelDoor>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelPlant>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelTorch>();
	UReferencePoolModuleBPLibrary::CreateReference<UVoxelWater>();

	UAssetModuleBPLibrary::LoadPrimaryAssets<UVoxelData>(FName("Voxel"));

	if(WorldMode != EVoxelWorldMode::None)
	{
		static FVoxelWorldSaveData SaveData = FVoxelWorldSaveData(WorldBasicData);
		LoadSaveData(&SaveData, WorldMode == EVoxelWorldMode::Normal);
	}
}

void AVoxelModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	switch(WorldState)
	{
		case EVoxelWorldState::Generating:
		case EVoxelWorldState::BasicGenerated:
		case EVoxelWorldState::FullGenerated:
		{
			GenerateTerrain();
			break;
		}
		default: break;
	}
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

	StopAsyncTasks();
}

float AVoxelModule::GetWorldLength() const
{
	return WorldData->ChunkSize * ChunkSpawnRange * 2;
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
	if(bForceMode)
	{
		if(WorldState == EVoxelWorldState::None)
		{
			WorldState = EVoxelWorldState::Generating;
			OnWorldStateChanged();
		}
	}

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
			GetWorldData().SetChunkData(Iter.Key, Iter.Value->ToSaveData<FVoxelChunkSaveData>());
		}
	}
	return WorldData;
}

void AVoxelModule::UnloadData(bool bForceMode)
{
	if(bForceMode)
	{
		if(WorldState != EVoxelWorldState::None)
		{
			WorldState = EVoxelWorldState::None;
			OnWorldStateChanged();
		}
		
		StopAsyncTasks();

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
		ChunkMapGenerateQueue.Empty();
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

void AVoxelModule::StopAsyncTasks()
{
	for(int32 i = 0; i < ChunkMapBuildTasks.Num(); i++)
	{
		if(ChunkMapBuildTasks[i])
		{
			ChunkMapBuildTasks[i]->Cancel();
			// delete ChunkMapBuildTasks[i];
		}
	}
	ChunkMapBuildTasks.Empty();
		
	for(int32 i = 0; i < ChunkMapGenerateTasks.Num(); i++)
	{
		if(ChunkMapGenerateTasks[i])
		{
			ChunkMapGenerateTasks[i]->Cancel();
			// delete ChunkMapGenerateTasks[i];
		}
	}
	ChunkMapGenerateTasks.Empty();
}

void AVoxelModule::GeneratePreviews()
{
	auto VoxelDatas = UAssetModuleBPLibrary::LoadPrimaryAssets<UVoxelData>(UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Voxel));
	if(VoxelDatas.Num() == 0) return;
	
	int32 tmpNum = 8;
	int32 tmpIndex = 0;
	VoxelsCapture->OrthoWidth = tmpNum * WorldData->BlockSize * 0.5f;
	for (float x = -(tmpNum - 1) * 0.5f; x <= (tmpNum - 1) * 0.5f; x++)
	{
		for (float y = -(tmpNum - 1) * 0.5f; y <= (tmpNum - 1) * 0.5f; y++)
		{
			if (tmpIndex >= VoxelDatas.Num()) break;
	
			auto voxelEntity = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelEntity>();
			if (voxelEntity != nullptr)
			{
				voxelEntity->Initialize(VoxelDatas[tmpIndex]->GetPrimaryAssetId());
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

void AVoxelModule::GenerateTerrain()
{
	if(IVoxelAgentInterface* VoxelAgent = UGlobalBPLibrary::GetPlayerPawn<IVoxelAgentInterface>())
	{
		const FIndex ChunkIndex = UVoxelModuleBPLibrary::LocationToChunkIndex(VoxelAgent->GetWorldLocation(), true);

		if(LastGenerateIndex == FIndex(-1, -1, -1) || FIndex::Distance(ChunkIndex, LastGenerateIndex, true) >= ChunkSpawnDistance)
		{
			GenerateChunks(ChunkIndex);
		}

		if(ChunkIndex != LastStayChunkIndex)
		{
			LastStayChunkIndex = ChunkIndex;
			for(auto iter = ChunkMap.CreateConstIterator(); iter; ++iter)
			{
				FIndex index = iter->Key;
				if(FIndex::Distance(ChunkIndex, index, true) >= ChunkSpawnRange + ChunkDestroyDistance)
				{
					AddToDestroyQueue(index);
				}
				else if(ChunkDestroyQueue.Contains(index))
				{
					ChunkDestroyQueue.Remove(index);
				}
			}
		}

		if(ChunkDestroyQueue.Num() > 0) //destroy queue
		{
			const int32 tmpNum = FMath::Min(ChunkDestroyQueue.Num(), ChunkDestroySpeed);
			for(int32 i = 0; i < tmpNum; i++)
			{
				DestroyChunk(ChunkDestroyQueue[i]);
			}
			ChunkDestroyQueue.RemoveAt(0, tmpNum);
		}
		else if(ChunkSpawnQueue.Num() > 0) //spawn queue
		{
			const int32 tmpNum = FMath::Min(ChunkSpawnQueue.Num(), ChunkSpawnSpeed);
			for(int32 i = 0; i < tmpNum; i++)
			{
				SpawnChunk(ChunkSpawnQueue[i]);
			}
			ChunkSpawnQueue.RemoveAt(0, tmpNum);
		}
		else if(ChunkMapBuildQueue.Num() > 0 || ChunkMapBuildTasks.Num() > 0) //map build queue 0
		{
			TArray<FIndex> tmpArr;
			const int32 tmpNum = FMath::Min(ChunkMapBuildQueue.Num(), ChunkMapBuildSpeed);
			for(int32 i = 0; i < tmpNum; i++)
			{
				FIndex index = ChunkMapBuildQueue[i];
				if (GetWorldData().IsExistChunkData(index))
				{
					AddToMapLoadQueue(index);
				}
				else
				{
					tmpArr.Add(index);
					AddToMapBuildQueue(index, 1);
				}
			}
			if(tmpArr.Num() > 0)
			{
				const auto tmpTask = new FAsyncTask<ChunkMapBuildTask>(this, tmpArr);
				ChunkMapBuildTasks.Add(tmpTask);
				tmpTask->StartBackgroundTask();
			}
			if(tmpNum > 0)
			{
				ChunkMapBuildQueue.RemoveAt(0, tmpNum);
			}
			bool bIsAllChunkBuilded = true;
			for(int32 i = 0; i < ChunkMapBuildTasks.Num(); i++)
			{
				if(ChunkMapBuildTasks[i] && ChunkMapBuildTasks[i]->IsDone())
				{
					delete ChunkMapBuildTasks[i];
					ChunkMapBuildTasks[i] = nullptr;
				}
				if(ChunkMapBuildTasks[i])
				{
					bIsAllChunkBuilded = false;
				}
			}
			if(bIsAllChunkBuilded)
			{
				ChunkMapBuildTasks.Empty();
			}
		}
		else if(ChunkMapLoadQueue.Num() > 0) //map load queue
		{
			const int32 tmpNum = FMath::Min(ChunkMapLoadQueue.Num(), ChunkMapLoadSpeed);
			for(int32 i = 0; i < tmpNum; i++)
			{
				LoadChunkMap(ChunkMapLoadQueue[i]);
			}
			ChunkMapLoadQueue.RemoveAt(0, tmpNum);
		}
		else if(ChunkMapBuildQueue1.Num() > 0) //map build queue 1
		{
			const int32 tmpNum = FMath::Min(ChunkMapBuildQueue1.Num(), ChunkMapBuildSpeed);
			for(int32 i = 0; i < tmpNum; i++)
			{
				BuildChunkMap(ChunkMapBuildQueue1[i], 1);
			}
			ChunkMapBuildQueue1.RemoveAt(0, tmpNum);
		}
		else if(ChunkMapGenerateQueue.Num() > 0 || ChunkMapGenerateTasks.Num() > 0) //map generate queue
		{
			TArray<FIndex> tmpArr;
			const int32 tmpNum = FMath::Min(ChunkMapGenerateQueue.Num(), ChunkMapGenerateSpeed);
			for(int32 i = 0; i < tmpNum; i++)
			{
				tmpArr.Add(ChunkMapGenerateQueue[i]);
			}
			if(tmpArr.Num() > 0)
			{
				const auto tmpTask = new FAsyncTask<ChunkMapGenerateTask>(this, tmpArr);
				ChunkMapGenerateTasks.Add(tmpTask);
				tmpTask->StartBackgroundTask();
			}
			if(tmpNum > 0)
			{
				ChunkMapGenerateQueue.RemoveAt(0, tmpNum);
			}
			bool bIsAllChunkGenerated = true;
			for(int32 i = 0; i < ChunkMapGenerateTasks.Num(); i++)
			{
				if(ChunkMapGenerateTasks[i] && ChunkMapGenerateTasks[i]->IsDone())
				{
					delete ChunkMapGenerateTasks[i];
					ChunkMapGenerateTasks[i] = nullptr;
				}
				if(ChunkMapGenerateTasks[i])
				{
					bIsAllChunkGenerated = false;
				}
			}
			if(bIsAllChunkGenerated)
			{
				ChunkMapGenerateTasks.Empty();

				ChunkGenerateQueue.Sort([ChunkIndex](const FIndex& A, const FIndex& B){
					float lengthA = FIndex::Distance(ChunkIndex, A, true);
					float lengthB = FIndex::Distance(ChunkIndex, B, true);
					if(lengthA == lengthB) return A.Z < B.Z;
					return lengthA < lengthB;
				});
			}
		}
		else if(ChunkGenerateQueue.Num() > 0) //generate queue
		{
			const int32 tmpNum = FMath::Min(ChunkGenerateQueue.Num(), ChunkGenerateSpeed);
			for(int32 i = 0; i < tmpNum; i++)
			{
				GenerateChunk(ChunkGenerateQueue[i]);
			}
			ChunkGenerateQueue.RemoveAt(0, tmpNum);

			const int32 fullNum = FMath::Square(ChunkSpawnRange * 2) * WorldData->ChunkHeightRange;
			const int32 basicNum = FMath::Square(ChunkBasicSpawnRange * 2) * WorldData->ChunkHeightRange;
			const int32 generatedNum = fullNum - ChunkGenerateQueue.Num();
			if(generatedNum == fullNum)
			{
				if(WorldState != EVoxelWorldState::FullGenerated)
				{
					WorldState = EVoxelWorldState::FullGenerated;
					OnWorldStateChanged();
				}
			}
			else if(generatedNum >= basicNum)
			{
				if(WorldState != EVoxelWorldState::BasicGenerated)
				{
					WorldState = EVoxelWorldState::BasicGenerated;
					OnWorldStateChanged();
				}
			}
		}
	}
}

void AVoxelModule::GenerateChunks(FIndex InIndex)
{
	if(ChunkSpawnQueue.Num() > 0) return;

	for(int32 x = InIndex.X - ChunkSpawnRange; x < InIndex.X + ChunkSpawnRange; x++)
	{
		for(int32 y = InIndex.Y - ChunkSpawnRange; y < InIndex.Y + ChunkSpawnRange; y++)
		{
			for(int32 z = 0; z < WorldData->ChunkHeightRange; z++)
			{
				if(FIndex::Distance(FIndex(x, y, z), InIndex, true) < ChunkSpawnRange + ChunkDestroyDistance)
				{
					AddToSpawnQueue(FIndex(x, y, z));
				}
			}
		}
	}
	LastGenerateIndex = InIndex;
	ChunkSpawnBatch++;
}

void AVoxelModule::LoadChunkMap(FIndex InIndex)
{
	if (!ChunkMap.Contains(InIndex)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->LoadSaveData(GetWorldData().GetChunkData(InIndex));

	AddToMapGenerateQueue(InIndex);
}

void AVoxelModule::BuildChunkMap(FIndex InIndex, int32 InStage)
{
	if (!ChunkMap.Contains(InIndex)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->BuildMap(InStage);

	AddToMapGenerateQueue(InIndex);
}

void AVoxelModule::GenerateChunkMap(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->GenerateMap();

	AddToGenerateQueue(InIndex);
}

void AVoxelModule::GenerateChunk(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->Generate(WorldMode == EVoxelWorldMode::Normal);
}

void AVoxelModule::DestroyChunk(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return;

	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	if(ChunkMapLoadQueue.Contains(InIndex))
	{
		ChunkMapLoadQueue.Remove(InIndex);
	}

	if(ChunkMapBuildQueue.Contains(InIndex))
	{
		ChunkMapBuildQueue.Remove(InIndex);
	}

	if(ChunkMapBuildQueue1.Contains(InIndex))
	{
		ChunkMapBuildQueue1.Remove(InIndex);
	}

	if(ChunkMapGenerateQueue.Contains(InIndex))
	{
		ChunkMapGenerateQueue.Remove(InIndex);
	}

	if(ChunkGenerateQueue.Contains(InIndex))
	{
		ChunkGenerateQueue.Remove(InIndex);
	}

	ChunkMap.Remove(InIndex);

	GetWorldData().SetChunkData(InIndex, Chunk->ToSaveData<FVoxelChunkSaveData>());

	UObjectPoolModuleBPLibrary::DespawnObject(Chunk);
}

void AVoxelModule::AddToSpawnQueue(FIndex InIndex)
{
	if(ChunkMap.Contains(InIndex)) return;
	
	ChunkSpawnQueue.AddUnique(InIndex);
}

void AVoxelModule::AddToMapLoadQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return;
	
	ChunkMapLoadQueue.AddUnique(InIndex);
}

void AVoxelModule::AddToMapBuildQueue(FIndex InIndex, int32 InStage)
{
	if(!ChunkMap.Contains(InIndex)) return;

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
}

void AVoxelModule::AddToMapGenerateQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return;
	
	ChunkMapGenerateQueue.AddUnique(InIndex);
}

void AVoxelModule::AddToGenerateQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return;
	
	ChunkGenerateQueue.AddUnique(InIndex);
}

void AVoxelModule::AddToDestroyQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return;
	
	ChunkDestroyQueue.AddUnique(InIndex);
}

AVoxelChunk* AVoxelModule::SpawnChunk(FIndex InIndex, bool bAddToQueue)
{
	if(ChunkMap.Contains(InIndex)) return ChunkMap[InIndex];
	
	AVoxelChunk* Chunk = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelChunk>(nullptr, ChunkSpawnClass);
	if(Chunk)
	{
		Chunk->SetActorLocationAndRotation(InIndex.ToVector() * WorldData->GetChunkLength(), FRotator::ZeroRotator);

		ChunkMap.Add(InIndex, Chunk);

		Chunk->Initialize(this, InIndex, ChunkSpawnBatch);
		Chunk->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

		if(bAddToQueue)
		{
			AddToMapBuildQueue(InIndex, 0);

			for(auto Iter : Chunk->GetNeighbors())
			{
				if(Iter && Iter->GetBatch() != Chunk->GetBatch())
				{
					AddToGenerateQueue(Iter->GetIndex());
				}
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
