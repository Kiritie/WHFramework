// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModule.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Character/Base/CharacterBase.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Voxel/EventHandle_ChangeVoxelWorldState.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Main/MainModule.h"
#include "Math/MathBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Scene/SceneModule.h"
#include "Scene/Components/WorldTimerComponent.h"
#include "ReferencePool/ReferencePoolModuleBPLibrary.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Datas/VoxelData.h"

AVoxelModule* AVoxelModule::Current = nullptr;
FVoxelWorldSaveData* AVoxelModule::WorldData = nullptr;

// Sets default values
AVoxelModule::AVoxelModule()
{
	ModuleName = FName("VoxelModule");

	VoxelsCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("VoxelsCapture"));
	VoxelsCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	VoxelsCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	VoxelsCapture->SetupAttachment(RootComponent);
	VoxelsCapture->SetRelativeLocationAndRotation(FVector(0, 0, -500), FRotator(90, 0, 0));

	ChunkSpawnRange = 7;
	ChunkBasicSpawnRange = 1;
	ChunkSpawnDistance = 3;

	ChunkSpawnSpeed = 1000;
	ChunkDestroyDistance = 1;
	ChunkDestroySpeed = 10;
	ChunkMapBuildSpeed = 10;
	ChunkMapGenerateSpeed = 10;
	ChunkGenerateSpeed = 1;

	ChunkSpawnBatch = 0;
	LastGenerateIndex = FIndex(-1, -1, -1);
	LastStayChunkIndex = FIndex::ZeroIndex;
	ChunkMap = TMap<FIndex, AVoxelChunk*>();
	ChunkSpawnQueue = TArray<FIndex>();
	ChunkMapBuildQueue = TArray<AVoxelChunk*>();
	ChunkGenerateQueue = TArray<AVoxelChunk*>();
	ChunkDestroyQueue = TArray<AVoxelChunk*>();

	Current = this;
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

	UVoxel::EmptyVoxel = UVoxel::SpawnVoxel(EVoxelType::Empty);
	UVoxel::UnknownVoxel = UVoxel::SpawnVoxel(EVoxelType::Unknown);
	
	UObjectPoolModuleBPLibrary::DespawnObject(UObjectPoolModuleBPLibrary::SpawnObject<AVoxelChunk>());
}

void AVoxelModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
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
}

AVoxelModule* AVoxelModule::Get()
{
	return Current;
}

FVoxelWorldSaveData* AVoxelModule::GetWorldData()
{
	if(WorldData)
	{
		return WorldData;
	}
	else
	{
		return &FVoxelWorldSaveData::Empty;
	}
}

float AVoxelModule::GetWorldLength() const
{
	return GetWorldData()->ChunkSize * ChunkSpawnRange * 2;
}

bool AVoxelModule::ChangeWorldState(EVoxelWorldState InWorldState)
{
	if(WorldState != InWorldState)
	{
		WorldState = InWorldState;
		switch(WorldState)
		{
			case EVoxelWorldState::BasicGenerated:
			{
				break;
			}
			case EVoxelWorldState::FullGenerated:
			{
				break;
			}
			default: break;
		}
		UEventModuleBPLibrary::BroadcastEvent(UEventHandle_ChangeVoxelWorldState::StaticClass(), EEventNetType::Single, this, TArray<FParameter> { FParameter::MakePointer(&WorldState) });
		return true;
	}
	return false;
}

void AVoxelModule::InitRandomStream(int32 InDeltaSeed)
{
	RandomStream.Initialize(GetWorldData()->WorldSeed + InDeltaSeed);
}

void AVoxelModule::LoadData(FSaveData* InWorldData)
{
	ChangeWorldState(EVoxelWorldState::Generating);
	
	WorldData = static_cast<FVoxelWorldSaveData*>(InWorldData);
	RandomStream = FRandomStream(GetWorldData()->WorldSeed);
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		if(UWorldTimerComponent* WorldTimer = SceneModule->GetWorldTimer())
		{
			WorldTimer->SetTimeSeconds(GetWorldData()->TimeSeconds);
		}
	}
}

FSaveData* AVoxelModule::ToData(bool bSaved)
{
	return WorldData;
}

void AVoxelModule::UnloadData(bool bPreview)
{
	if(!bPreview)
	{
		ChangeWorldState(EVoxelWorldState::None);
		
		for (auto iter : ChunkMap)
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
		ChunkMapBuildQueue.Empty();
		ChunkMapGenerateQueue.Empty();
		ChunkGenerateQueue.Empty();
		ChunkDestroyQueue.Empty();

		WorldData = nullptr;
	}
	else
	{
		for(auto Iter : ChunkMap)
		{
			if(Iter.Value && Iter.Value->IsGenerated())
			{
				Iter.Value->DestroyActors();
			}
		}
	}
}

void AVoxelModule::GeneratePreviews()
{
	// auto voxelDatas = UDWHelper::LoadVoxelDatas();
	// if(voxelDatas.Num() == 0) return;
	//
	// int32 tmpNum = 8;
	// int32 tmpIndex = 0;
	// VoxelsCapture->OrthoWidth = tmpNum * GetWorldData()->BlockSize * 0.5f;
	// for (float x = -(tmpNum - 1) * 0.5f; x <= (tmpNum - 1) * 0.5f; x++)
	// {
	// 	for (float y = -(tmpNum - 1) * 0.5f; y <= (tmpNum - 1) * 0.5f; y++)
	// 	{
	// 		if (tmpIndex >= voxelDatas.Num()) break;
	//
	// 		//voxelDatas[tmpIndex].TexUV = FVector2D((tmpIndex - 1) % tmpNum, (tmpIndex - 1) / tmpNum);
	// 		FActorSpawnParameters spawnParams = FActorSpawnParameters();
	// 		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 		auto pickUpItem = GetWorld()->SpawnActor<APickUpVoxel>(spawnParams);
	// 		if (pickUpItem != nullptr)
	// 		{
	// 			pickUpItem->Initialize(FAbilityItem(voxelDatas[tmpIndex].ID, 1), true);
	// 			pickUpItem->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
	// 			pickUpItem->SetActorLocation(FVector(x * GetWorldData()->BlockSize * 0.5f, y * GetWorldData()->BlockSize * 0.5f, 0));
	// 			pickUpItem->SetActorRotation(FRotator(-70, 0, -180));
	// 			pickUpItem->GetMeshComponent()->SetRelativeRotation(FRotator(0, 45, 0));
	// 			VoxelsCapture->ShowOnlyActors.Add(pickUpItem);
	// 		}
	//
	// 		tmpIndex++;
	// 	}
	// }
}

void AVoxelModule::GenerateTerrain()
{
	if(IWHPlayerInterface* Player = UGlobalBPLibrary::GetPlayerPawn<IWHPlayerInterface>(this))
	{
		FIndex chunkIndex = UVoxelModuleBPLibrary::LocationToChunkIndex(Cast<AActor>(Player)->GetActorLocation(), true);

		if (LastGenerateIndex == FIndex(-1, -1, -1) || FIndex::Distance(chunkIndex, LastGenerateIndex, true) >= ChunkSpawnDistance)
		{
			GenerateChunks(chunkIndex);
		}

		if (chunkIndex != LastStayChunkIndex)
		{
			LastStayChunkIndex = chunkIndex;
			for (auto iter = ChunkMap.CreateConstIterator(); iter; ++iter)
			{
				FIndex index = iter->Key;
				AVoxelChunk* chunk = iter->Value;
				if (FIndex::Distance(chunkIndex, index, true) >= ChunkSpawnRange + ChunkDestroyDistance)
				{
					AddToDestroyQueue(chunk);
				}
				else if (ChunkDestroyQueue.Contains(chunk))
				{
					ChunkDestroyQueue.Remove(chunk);
				}
			}
		}

		if (ChunkDestroyQueue.Num() > 0)
		{
			const int32 tmpNum = FMath::Min(ChunkDestroyQueue.Num(), ChunkDestroySpeed);
			for (int32 i = 0; i < tmpNum; i++)
			{
				DestroyChunk(ChunkDestroyQueue[i]);
			}
			ChunkDestroyQueue.RemoveAt(0, tmpNum);
		}
		else if (ChunkSpawnQueue.Num() > 0)
		{
			const int32 tmpNum = FMath::Min(ChunkSpawnQueue.Num(), ChunkSpawnSpeed);
			for (int32 i = 0; i < tmpNum; i++)
			{
				SpawnChunk(ChunkSpawnQueue[i]);
			}
			ChunkSpawnQueue.RemoveAt(0, tmpNum);
		}
		else if (ChunkMapBuildQueue.Num() > 0)
		{
			// TArray<AVoxelChunk*> tmpArr;
			// for(int32 i = 0; i < ChunkMapBuildQueue.Num(); i++)
			// {
			// 	tmpArr.Add(ChunkMapBuildQueue[i]);
			// 	if(tmpArr.Num() >= FMath::Min(ChunkMapBuildQueue.Num(), ChunkMapBuildSpeed))
			// 	{
			// 		FAsyncTask<ChunkMapBuildTask>* tmpTask = new FAsyncTask<ChunkMapBuildTask>(this, tmpArr);
			// 		tmpTask->StartBackgroundTask();
			// 		ChunkMapBuildTasks.Add(tmpTask);
			// 		tmpArr.Empty();
			// 	}
			// }
			// for(int32 i = 0; i < ChunkMapBuildTasks.Num(); i++)
			// {
			// 	FAsyncTask<ChunkMapBuildTask>* tmpTask = ChunkMapBuildTasks[i];
			// 	if(tmpTask && tmpTask->IsDone())
			// 	{
			// 		for(auto iter : tmpTask->GetTask().GetQueue())
			// 		{
			// 			if(ChunkMapBuildQueue.Contains(iter))
			// 			{
			// 				ChunkMapBuildQueue.Remove(iter);
			// 			}
			// 		}
			// 		delete tmpTask;
			// 	}
			// }
			const int32 tmpNum = FMath::Min(ChunkMapBuildQueue.Num(), ChunkMapBuildSpeed);
			for (int32 i = 0; i < tmpNum; i++)
			{
				BuildChunkMap(ChunkMapBuildQueue[i]);
			}
			ChunkMapBuildQueue.RemoveAt(0, tmpNum);
		}
		else if (ChunkMapGenerateQueue.Num() > 0)
		{
			// if(ChunkMapBuildTasks.Num() > 0)
			// {
			// 	ChunkMapBuildTasks.Empty();
			// }
			const int32 tmpNum = FMath::Min(ChunkMapGenerateQueue.Num(), ChunkMapGenerateSpeed);
			for (int32 i = 0; i < tmpNum; i++)
			{
				GenerateChunkMap(ChunkMapGenerateQueue[i]);
			}
			ChunkMapGenerateQueue.RemoveAt(0, tmpNum);
		}
		else if (ChunkGenerateQueue.Num() > 0)
		{
			const int32 tmpNum = FMath::Min(ChunkGenerateQueue.Num(), ChunkGenerateSpeed);
			for (int32 i = 0; i < tmpNum; i++)
			{
				GenerateChunk(ChunkGenerateQueue[i]);
			}
			ChunkGenerateQueue.RemoveAt(0, tmpNum);

			const float FullNum = FMath::Square(ChunkSpawnRange * 2) * GetWorldData()->ChunkHeightRange;
			const float BasicNum = FMath::Square(ChunkBasicSpawnRange * 2) * GetWorldData()->ChunkHeightRange;
			if(FullNum - ChunkGenerateQueue.Num() >= BasicNum)
			{
				ChangeWorldState(EVoxelWorldState::BasicGenerated);
			}
			if(ChunkGenerateQueue.Num() == 0)
			{
				ChangeWorldState(EVoxelWorldState::FullGenerated);
			}
		}
	}
}

void AVoxelModule::GenerateChunks(FIndex InIndex)
{
	if (ChunkSpawnQueue.Num() > 0) return;

	for (int32 x = InIndex.X - ChunkSpawnRange; x < InIndex.X + ChunkSpawnRange; x++)
	{
		for (int32 y = InIndex.Y - ChunkSpawnRange; y < InIndex.Y + ChunkSpawnRange; y++)
		{
			for (int32 z = 0; z < GetWorldData()->ChunkHeightRange ; z++)
			{
				AddToSpawnQueue(FIndex(x, y, z));
			}
		}
	}

	ChunkSpawnQueue.Sort([InIndex](const FIndex& A, const FIndex& B) {
		float lengthA = FIndex::Distance(InIndex, A, true);
		float lengthB = FIndex::Distance(InIndex, B, true);
		if (lengthA == lengthB) return A.Z < B.Z;
		return lengthA < lengthB;
	});

	LastGenerateIndex = InIndex;
	ChunkSpawnBatch++;
}

void AVoxelModule::BuildChunkMap(AVoxelChunk* InChunk)
{
	if (!InChunk || !ChunkMap.Contains(InChunk->GetIndex())) return;

	InChunk->BuildMap();

	AddToMapGenerateQueue(InChunk);
}

void AVoxelModule::GenerateChunkMap(AVoxelChunk* InChunk)
{
	if (!InChunk || !ChunkMap.Contains(InChunk->GetIndex())) return;

	InChunk->GenerateMap();

	AddToGenerateQueue(InChunk);
}

void AVoxelModule::GenerateChunk(AVoxelChunk* InChunk)
{
	if (!InChunk || !ChunkMap.Contains(InChunk->GetIndex())) return;
	
	InChunk->Generate(false);
}

void AVoxelModule::DestroyChunk(AVoxelChunk* InChunk)
{
	if (!InChunk || !ChunkMap.Contains(InChunk->GetIndex())) return;

	if (ChunkMapBuildQueue.Contains(InChunk))
	{
		ChunkMapBuildQueue.Remove(InChunk);
	}

	if (ChunkMapGenerateQueue.Contains(InChunk))
	{
		ChunkMapGenerateQueue.Remove(InChunk);
	}

	if (ChunkGenerateQueue.Contains(InChunk))
	{
		ChunkGenerateQueue.Remove(InChunk);
	}

	ChunkMap.Remove(InChunk->GetIndex());

	UObjectPoolModuleBPLibrary::DespawnObject(InChunk);
}

void AVoxelModule::AddToSpawnQueue(FIndex InIndex)
{
	if (!ChunkMap.Contains(InIndex) && !ChunkSpawnQueue.Contains(InIndex))
	{
		ChunkSpawnQueue.Add(InIndex);
	}
}

void AVoxelModule::AddToMapBuildQueue(AVoxelChunk* InChunk)
{
	if (ChunkMap.Contains(InChunk->GetIndex()) && !ChunkMapBuildQueue.Contains(InChunk))
	{
		ChunkMapBuildQueue.Add(InChunk);
	}
}

void AVoxelModule::AddToMapGenerateQueue(AVoxelChunk* InChunk)
{
	if (ChunkMap.Contains(InChunk->GetIndex()) && !ChunkMapGenerateQueue.Contains(InChunk))
	{
		ChunkMapGenerateQueue.Add(InChunk);
	}
}

void AVoxelModule::AddToGenerateQueue(AVoxelChunk* InChunk)
{
	if (ChunkMap.Contains(InChunk->GetIndex()) && !ChunkGenerateQueue.Contains(InChunk))
	{
		ChunkGenerateQueue.Add(InChunk);
	}
}

void AVoxelModule::AddToDestroyQueue(AVoxelChunk* InChunk)
{
	if (ChunkMap.Contains(InChunk->GetIndex()) && !ChunkDestroyQueue.Contains(InChunk))
	{
		ChunkDestroyQueue.Add(InChunk);
	}
}

AVoxelChunk* AVoxelModule::SpawnChunk(FIndex InIndex, bool bAddToQueue)
{
	if (ChunkMap.Contains(InIndex)) return ChunkMap[InIndex];

	auto chunk = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelChunk>();
	if (chunk)
	{
		chunk->SetActorLocationAndRotation(InIndex.ToVector() * GetWorldData()->GetChunkLength(), FRotator::ZeroRotator);
		
		ChunkMap.Add(InIndex, chunk);

		chunk->Initialize(InIndex, ChunkSpawnBatch);
		chunk->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

		if (bAddToQueue)
		{
			AddToMapBuildQueue(chunk);

			for(auto Iter : chunk->GetNeighbors())
			{
				if(Iter && Iter->GetBatch() != chunk->GetBatch())
				{
					AddToGenerateQueue(Iter);
				}
			}
		}
	}
	return chunk;
}

AVoxelChunk* AVoxelModule::FindChunk(FVector InLocation)
{
	return FindChunk(UVoxelModuleBPLibrary::LocationToChunkIndex(InLocation));
}

AVoxelChunk* AVoxelModule::FindChunk(FIndex InIndex)
{
	if (ChunkMap.Contains(InIndex))
	{
		return ChunkMap[InIndex];
	}
	return nullptr;
}

EVoxelType AVoxelModule::GetNoiseVoxelType(FIndex InIndex)
{
	const FVector offsetIndex = FVector(InIndex.X + GetWorldData()->WorldSeed, InIndex.Y + GetWorldData()->WorldSeed, InIndex.Z);
	
	const int plainHeight = GetNoiseTerrainHeight(offsetIndex, GetWorldData()->TerrainPlainScale);
	const int mountainHeight = GetNoiseTerrainHeight(offsetIndex, GetWorldData()->TerrainMountainScale);
	
	const int baseHeight = FMath::Clamp(FMath::Max(plainHeight, mountainHeight) + int32(GetWorldData()->GetWorldHeight() * GetWorldData()->TerrainBaseHeight), 0, GetWorldData()->GetWorldHeight() - 1);

	const int stoneHeight = FMath::Clamp(GetNoiseTerrainHeight(offsetIndex, GetWorldData()->TerrainStoneVoxelScale), 0, GetWorldData()->GetWorldHeight() - 1);
	const int sandHeight = FMath::Clamp(GetNoiseTerrainHeight(offsetIndex, GetWorldData()->TerrainSandVoxelScale), 0, GetWorldData()->GetWorldHeight() - 1);

	const int waterHeight = FMath::Clamp(int32(GetWorldData()->GetWorldHeight() * GetWorldData()->TerrainWaterVoxelHeight), 0, GetWorldData()->GetWorldHeight() - 1);
	const int bedrockHeight = FMath::Clamp(int32(GetWorldData()->GetWorldHeight() * GetWorldData()->TerrainBedrockVoxelHeight), 0, GetWorldData()->GetWorldHeight() - 1);
	
	if (InIndex.Z < baseHeight)
	{
		if (InIndex.Z <= bedrockHeight)
		{
			return EVoxelType::Bedrock; //Bedrock
		}
		else if (InIndex.Z <= stoneHeight)
		{
			return EVoxelType::Stone; //Stone
		}
		return EVoxelType::Dirt; //Dirt
	}
	else if(InIndex.Z <= sandHeight)
	{
		return EVoxelType::Sand; //Sand
	}
	else if (InIndex.Z <= waterHeight)
	{
		return EVoxelType::Water; //Water
	}
	else if (InIndex.Z == baseHeight)
	{
		return EVoxelType::Grass; //Grass
	}
	return EVoxelType::Empty; //Empty
}

UVoxelData* AVoxelModule::GetNoiseVoxelData(FIndex InIndex)
{
	return UAssetModuleBPLibrary::LoadPrimaryAsset<UVoxelData>(FPrimaryAssetId::FromString(*FString::Printf(TEXT("Voxel_%d"), (int32)GetNoiseVoxelType(InIndex))));
}

int AVoxelModule::GetNoiseTerrainHeight(FVector InOffset, FVector InScale)
{
	return (FMath::PerlinNoise2D(FVector2D(InOffset.X * InScale.X, InOffset.Y * InScale.Y)) + 1) * GetWorldData()->GetWorldHeight() * InScale.Z;
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
	for (auto iter = ChunkMap.CreateConstIterator(); iter; ++iter)
	{
		if(AVoxelChunk* chunk = iter->Value)
		{
			if (!bNeedGenerated || chunk->IsGenerated())
			{
				num++;
			}
		}
	}
	return num;
}
