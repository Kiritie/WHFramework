// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModule.h"

#include "ImageUtils.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Character/Base/CharacterBase.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Voxel/EventHandle_ChangeVoxelWorldState.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Math/MathBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Scene/SceneModule.h"
#include "Scene/Components/WorldTimerComponent.h"
#include "ReferencePool/ReferencePoolModuleBPLibrary.h"
#include "Scene/SceneModuleBPLibrary.h"
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
#include "Voxel/Voxels/Entity/VoxelEntityPreview.h"
#include "Global/GlobalBPLibrary.h"

IMPLEMENTATION_MODULE(AVoxelModule)

// Sets default values
AVoxelModule::AVoxelModule()
{
	ModuleName = FName("VoxelModule");

	VoxelsCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("VoxelsCapture"));
	VoxelsCapture->SetupAttachment(RootComponent);
	VoxelsCapture->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -1000.f), FRotator(90.f, 0.f, 0.f));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> VoxelsPreviewTexFinder(TEXT("TextureRenderTarget2D'/WHFramework/Voxel/Textures/RT_VoxelPreview.RT_VoxelPreview'"));
	if(VoxelsPreviewTexFinder.Succeeded())
	{
		VoxelsCapture->TextureTarget = VoxelsPreviewTexFinder.Object;
	}
	VoxelsCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	VoxelsCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	VoxelsCapture->bCaptureEveryFrame = true;

	bAutoGenerate = false;
	WorldMode = EVoxelWorldMode::None;
	WorldState = EVoxelWorldState::None;
	WorldBasicData = FVoxelWorldBasicSaveData();

	ChunkSpawnClass = AVoxelChunk::StaticClass();
	
	ChunkSpawnRange = 7;
	ChunkBasicSpawnRange = 1;
	ChunkSpawnDistance = 2;

	ChunkSpawnSpeed = 100;
	ChunkDestroyDistance = 1;
	ChunkDestroySpeed = 10;
	ChunkGenerateSpeed = 1;
	ChunkMapLoadSpeed = 100;
	ChunkMapBuildSpeed = 100;
	ChunkMeshBuildSpeed = 100;
	ChunkDataSaveSpeed = 50;

	ChunkSpawnBatch = 0;
	LastGenerateIndex = Index_Empty;
	LastStayChunkIndex = Index_Empty;
	ChunkMap = TMap<FIndex, AVoxelChunk*>();
	ChunkSpawnQueue = TArray<FIndex>();
	ChunkDestroyQueue = TArray<FIndex>();
	ChunkGenerateQueue = TArray<FIndex>();
	ChunkMapLoadQueue = TArray<FIndex>();
	ChunkMapBuildQueue = TArray<FIndex>();
	ChunkMapBuildQueue1 = TArray<FIndex>();
	ChunkMeshBuildQueue = TArray<FIndex>();
	ChunkDataSaveQueue = TArray<FIndex>();

	VoxelClasses.Add(UVoxel::StaticClass());
	VoxelClasses.Add(UVoxelEmpty::StaticClass());
	VoxelClasses.Add(UVoxelUnknown::StaticClass());
	VoxelClasses.Add(UVoxelDoor::StaticClass());
	VoxelClasses.Add(UVoxelPlant::StaticClass());
	VoxelClasses.Add(UVoxelTorch::StaticClass());
	VoxelClasses.Add(UVoxelWater::StaticClass());
	
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SolidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_Solid.M_Voxels_Solid'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitSolidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_Solid_Unlit.M_Voxels_Solid_Unlit'"));
	if(SolidMatFinder.Succeeded() && UnlitSolidMatFinder.Succeeded())
	{
		WorldBasicData.ChunkMaterials.Add(FVoxelChunkMaterial(SolidMatFinder.Object, UnlitSolidMatFinder.Object, FVector2D(0.0625f, 0.5f)));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SemiTransparentMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_SemiTransparent.M_Voxels_SemiTransparent'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitSemiTransparentMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_SemiTransparent_Unlit.M_Voxels_SemiTransparent_Unlit'"));
	if(SemiTransparentMatFinder.Succeeded() && UnlitSemiTransparentMatFinder.Succeeded())
	{
		WorldBasicData.ChunkMaterials.Add(FVoxelChunkMaterial(SemiTransparentMatFinder.Object, UnlitSemiTransparentMatFinder.Object, FVector2D(0.0625f, 0.5f)));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TransparentMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_Transparent.M_Voxels_Transparent'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitTransparentMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_Transparent_Unlit.M_Voxels_Transparent_Unlit'"));
	if(TransparentMatFinder.Succeeded() && UnlitTransparentMatFinder.Succeeded())
	{
		WorldBasicData.ChunkMaterials.Add(FVoxelChunkMaterial(TransparentMatFinder.Object, UnlitTransparentMatFinder.Object, FVector2D(0.0625f, 0.5f)));
	}
}

AVoxelModule::~AVoxelModule()
{
	TERMINATION_MODULE(AVoxelModule)
}

#if WITH_EDITOR
void AVoxelModule::OnGenerate()
{
	Super::OnGenerate();
}

void AVoxelModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AVoxelModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	UAbilityModuleBPLibrary::AddCustomInteractAction((int32)EVoxelInteractAction::Open, TEXT("/Script/WHFramework.EVoxelInteractAction"));
	UAbilityModuleBPLibrary::AddCustomInteractAction((int32)EVoxelInteractAction::Close, TEXT("/Script/WHFramework.EVoxelInteractAction"));
}

void AVoxelModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(InPhase == EPhase::Primary)
	{
		UAssetModuleBPLibrary::LoadPrimaryAssets<UVoxelData>(UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Voxel));

		for(auto Iter : VoxelClasses)
		{
			UReferencePoolModuleBPLibrary::CreateReference(nullptr, Iter);
		}
	}
	else if(InPhase == EPhase::Final)
	{
		GenerateVoxels();

		if(bAutoGenerate)
		{
			static FVoxelWorldSaveData SaveData;
			SaveData = FVoxelWorldSaveData(WorldBasicData);
			LoadSaveData(&SaveData, EPhase::Primary);
		}
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

	ChunkMap.Empty();
	StopChunkTasks(ChunkMapLoadTasks);
	StopChunkTasks(ChunkMapBuildTasks);
	StopChunkTasks(ChunkMeshBuildTasks);
}

void AVoxelModule::SetWorldMode(EVoxelWorldMode InWorldMode)
{
	if(WorldMode != InWorldMode)
	{
		WorldMode = InWorldMode;
	}
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

void AVoxelModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FVoxelWorldSaveData>();

	switch(InPhase)
	{
		case EPhase::Primary:
		{
			SetWorldMode(EVoxelWorldMode::Preview);

			if(!WorldData)
			{
				WorldData = new FVoxelWorldSaveData();
			}

			WorldData->BlockSize = SaveData.BlockSize;
			WorldData->ChunkSize = SaveData.ChunkSize;
			WorldData->ChunkHeightRange = SaveData.ChunkHeightRange;
			WorldData->TerrainBaseHeight = SaveData.TerrainBaseHeight;
			WorldData->TerrainPlainScale = SaveData.TerrainPlainScale;
			WorldData->TerrainMountainScale = SaveData.TerrainMountainScale;
			WorldData->TerrainStoneVoxelScale = SaveData.TerrainStoneVoxelScale;
			WorldData->TerrainSandVoxelScale = SaveData.TerrainSandVoxelScale;
			WorldData->TerrainPlantVoxelScale = SaveData.TerrainPlantVoxelScale;
			WorldData->TerrainTreeVoxelScale = SaveData.TerrainTreeVoxelScale;
			WorldData->TerrainBedrockVoxelHeight = SaveData.TerrainBedrockVoxelHeight;
			WorldData->TerrainWaterVoxelHeight = SaveData.TerrainWaterVoxelHeight;
			WorldData->ChunkMaterials = SaveData.ChunkMaterials;

			WorldData->WorldSeed = SaveData.WorldSeed;
			if(WorldData->WorldSeed == 0)
			{
				WorldData->WorldSeed = FMath::Rand();
			}
			WorldData->RandomStream = FRandomStream(WorldData->WorldSeed);
		}
		case EPhase::Lesser:
		{
			WorldData->TimeSeconds = SaveData.TimeSeconds;
			WorldData->SecondsOfDay = SaveData.SecondsOfDay;
			USceneModuleBPLibrary::GetWorldTimer()->InitializeTimer(WorldData->SecondsOfDay);
			USceneModuleBPLibrary::GetWorldTimer()->SetCurrentTime(WorldData->TimeSeconds);
			break;
		}
		case EPhase::Final:
		{
			SetWorldMode(EVoxelWorldMode::InGame);

			for(auto Iter : ChunkMap)
			{
				if(Iter.Value)
				{
					Iter.Value->Generate(EPhase::Final);
				}
			}
			break;
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
	WorldData->TimeSeconds = USceneModuleBPLibrary::GetWorldTimer()->GetTimeSeconds();
	WorldData->SecondsOfDay = USceneModuleBPLibrary::GetWorldTimer()->GetSecondsOfDay();
	return WorldData;
}

void AVoxelModule::UnloadData(EPhase InPhase)
{
	switch(InPhase)
	{
		case EPhase::Primary:
		{
			SetWorldMode(EVoxelWorldMode::None);
			SetWorldState(EVoxelWorldState::None);

			for(auto Iter : ChunkMap)
			{
				if(Iter.Value)
				{
					UObjectPoolModuleBPLibrary::DespawnObject(Iter.Value);
				}
			}
			ChunkMap.Empty();
		
			StopChunkTasks(ChunkMapLoadTasks);
			StopChunkTasks(ChunkMapBuildTasks);
			StopChunkTasks(ChunkMeshBuildTasks);

			ChunkSpawnBatch = 0;
			LastGenerateIndex = Index_Empty;
			LastStayChunkIndex = Index_Empty;

			ChunkSpawnQueue.Empty();
			ChunkMapLoadQueue.Empty();
			ChunkMapBuildQueue.Empty();
			ChunkMapBuildQueue1.Empty();
			ChunkMeshBuildQueue.Empty();
			ChunkGenerateQueue.Empty();
			ChunkDataSaveQueue.Empty();
			ChunkDestroyQueue.Empty();

			WorldData = nullptr;
			break;
		}
		case EPhase::Lesser:
		case EPhase::Final:
		{
			SetWorldMode(EVoxelWorldMode::Preview);

			for(auto Iter : ChunkMap)
			{
				if(Iter.Value && Iter.Value->IsGenerated())
				{
					Iter.Value->DestroyActors();
				}
			}
			break;
		}
	}
}

void AVoxelModule::GenerateWorld()
{
	const IVoxelAgentInterface* VoxelAgent = UGlobalBPLibrary::GetPlayerPawn<IVoxelAgentInterface>();

	if(!WorldData || !VoxelAgent || VoxelAgent->GetWorldLocation().Z < 0.f) return;
	
	const FIndex AgentIndex = LocationToChunkIndex(VoxelAgent->GetWorldLocation());

	// Check chunk spawn queue
	if(LastGenerateIndex == Index_Empty || LastGenerateIndex.DistanceTo(AgentIndex, true, true) >= ChunkSpawnDistance)
	{
		LastGenerateIndex = AgentIndex;
		for(int32 x = LastGenerateIndex.X - ChunkSpawnRange; x < LastGenerateIndex.X + ChunkSpawnRange; x++)
		{
			for(int32 y = LastGenerateIndex.Y - ChunkSpawnRange; y < LastGenerateIndex.Y + ChunkSpawnRange; y++)
			{
				for(int32 z = 0; z < WorldData->ChunkHeightRange; z++)
				{
					const FIndex index = FIndex(x, y, z);
					if(LastGenerateIndex.DistanceTo(index, true, true) < ChunkSpawnRange + ChunkDestroyDistance)
					{
						AddToSpawnQueue(index);
					}
				}
			}
		}
		ChunkSpawnBatch++;
	}
	// Check chunk destroy queue
	if(LastStayChunkIndex != AgentIndex)
	{
		LastStayChunkIndex = AgentIndex;
		for(auto iter = ChunkMap.CreateConstIterator(); iter; ++iter)
		{
			const FIndex index = iter->Key;
			if(LastStayChunkIndex.DistanceTo(index, true, true) >= ChunkSpawnRange + ChunkDestroyDistance)
			{
				AddToDestroyQueue(index);
			}
			else
			{
				RemoveFromDestroyQueue(index);
			}
		}
	}
	// Spawn chunk
	if(UpdateChunkQueue(ChunkSpawnQueue, ChunkSpawnSpeed, [this](FIndex Index){ SpawnChunk(Index); }))
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
	// Save chunk data
	else if(UpdateChunkTasks(ChunkDataSaveQueue, ChunkDataSaveTasks, ChunkDataSaveSpeed))
	{
		SetWorldState(EVoxelWorldState::SavingData);
	}
	// Destroy chunk
	else if(UpdateChunkQueue(ChunkDestroyQueue, ChunkDestroySpeed, [this](FIndex Index){ DestroyChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Destroying);
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
	VoxelsCapture->OrthoWidth = WorldBasicData.BlockSize * 4.f;
	
	int32 tmpIndex = 0;
	auto VoxelDatas = UAssetModuleBPLibrary::LoadPrimaryAssets<UVoxelData>(UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Voxel));
	for(int32 i = 0; i < VoxelDatas.Num(); i++)
	{
		if(VoxelDatas[i]->IsUnknown() || /*VoxelDatas[i]->IsCustom() || */VoxelDatas[i]->PartType != EVoxelPartType::Main) continue;
		
		if(AVoxelEntityPreview* VoxelEntity = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelEntityPreview>())
		{
			VoxelEntity->Initialize(VoxelDatas[i]->GetPrimaryAssetId());
			VoxelEntity->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
			VoxelEntity->SetActorLocation(FVector((tmpIndex / 8 - 3.5f) * WorldBasicData.BlockSize * 0.5f, (tmpIndex % 8 - 3.5f) * WorldBasicData.BlockSize * 0.5f, -800.f));
			VoxelEntity->SetActorRotation(FRotator(-70.f, 0.f, -180.f));
			VoxelsCapture->ShowOnlyActors.Add(VoxelEntity);

			VoxelDatas[i]->InitIconMat(VoxelsCapture->TextureTarget, 8, tmpIndex);

			tmpIndex++;
		}
	}
}

void AVoxelModule::LoadChunkMap(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		Chunk->LoadSaveData(WorldData->GetChunkData(InIndex));
	}
}

void AVoxelModule::BuildChunkMap(FIndex InIndex, int32 InStage)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		Chunk->BuildMap(InStage);
	}
}

void AVoxelModule::BuildChunkMesh(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		Chunk->BuildMesh();
	}
}

void AVoxelModule::SaveChunkData(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		WorldData->SetChunkData(InIndex, Chunk->ToSaveData<FVoxelChunkSaveData>());
	}
}

void AVoxelModule::GenerateChunk(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		switch(WorldMode)
		{
			case EVoxelWorldMode::InGame:
			{
				Chunk->Generate(EPhase::Primary);
				break;
			}
			case EVoxelWorldMode::Preview:
			{
				Chunk->Generate(EPhase::Lesser);
				break;
			}
			default: break;
		}
	}
}

void AVoxelModule::DestroyChunk(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		UObjectPoolModuleBPLibrary::DespawnObject(Chunk);
		ChunkMap.Remove(InIndex);
	}
}

bool AVoxelModule::AddToSpawnQueue(FIndex InIndex)
{
	if(ChunkMap.Contains(InIndex)) return false;

	RemoveFromDestroyQueue(InIndex);

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

bool AVoxelModule::AddToDataSaveQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return false;
	
	ChunkDataSaveQueue.AddUnique(InIndex);
	return true;
}

bool AVoxelModule::RemoveFromDataSaveQueue(FIndex InIndex)
{
	if(ChunkDataSaveQueue.Contains(InIndex))
	{
		ChunkDataSaveQueue.Remove(InIndex);
		return true;
	}
	return false;
}

bool AVoxelModule::AddToGenerateQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return false;

	ChunkGenerateQueue.AddUnique(InIndex);
	ChunkGenerateQueue.Sort([this](const FIndex& A, const FIndex& B){
		const float lengthA = LastGenerateIndex.DistanceTo(A, false, true);
		const float lengthB = LastGenerateIndex.DistanceTo(B, false, true);
		if(IsChunkGenerated(A) != IsChunkGenerated(B)) return !IsChunkGenerated(A);
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
	
	RemoveFromMapLoadQueue(InIndex);
	RemoveFromMapBuildQueue(InIndex, 0);
	RemoveFromMapBuildQueue(InIndex, 1);
	RemoveFromMeshBuildQueue(InIndex);
	RemoveFromDataSaveQueue(InIndex);
	RemoveFromGenerateQueue(InIndex);

	AddToDataSaveQueue(InIndex);

	ChunkDestroyQueue.AddUnique(InIndex);
	ChunkDestroyQueue.Sort([this](const FIndex& A, const FIndex& B){
		const float lengthA = LastGenerateIndex.DistanceTo(A, false, true);
		const float lengthB = LastGenerateIndex.DistanceTo(B, false, true);
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
	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);
	if(Chunk) return Chunk;
	
	Chunk = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelChunk>(nullptr, ChunkSpawnClass);
	if(Chunk)
	{
		ChunkMap.Add(InIndex, Chunk);

		Chunk->SetActorLocationAndRotation(InIndex.ToVector() * WorldData->GetChunkLength(), FRotator::ZeroRotator);
		Chunk->Initialize(InIndex, ChunkSpawnBatch);
		Chunk->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

		if(bAddToQueue)
		{
			if(WorldData->IsExistChunkData(InIndex))
			{
				AddToMapLoadQueue(InIndex);
			}
			else
			{
				DON(i, 2, AddToMapBuildQueue(InIndex, i);)
			}
			AddToMeshBuildQueue(InIndex);
			AddToGenerateQueue(InIndex);
			for(auto Iter : Chunk->GetNeighbors())
			{
				if(Iter.Value && Iter.Value->GetBatch() != Chunk->GetBatch())
				{
					AddToMeshBuildQueue(Iter.Value->GetIndex());
					AddToGenerateQueue(Iter.Value->GetIndex());
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
	return FindChunkByIndex(LocationToChunkIndex(InLocation));
}

FVoxelItem& AVoxelModule::FindVoxelByIndex(FIndex InIndex)
{
	return FindVoxelByLocation(VoxelIndexToLocation(InIndex));
}

FVoxelItem& AVoxelModule::FindVoxelByLocation(FVector InLocation)
{
	if(AVoxelChunk* Chunk = FindChunkByLocation(InLocation))
	{
		return Chunk->GetVoxelItem(Chunk->LocationToIndex(InLocation));
	}
	return FVoxelItem::Empty;
}

FIndex AVoxelModule::LocationToChunkIndex(FVector InLocation, bool bIgnoreZ /*= false*/)
{
	InLocation /= WorldData->GetChunkLength();
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), bIgnoreZ ? 0 : FMath::FloorToInt(InLocation.Z));
}

FVector AVoxelModule::ChunkIndexToLocation(FIndex InIndex)
{
	return InIndex.ToVector() * WorldData->GetChunkLength();
}

FIndex AVoxelModule::LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ)
{
	InLocation /= WorldData->BlockSize;
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), bIgnoreZ ? 0 : FMath::FloorToInt(InLocation.Z));
}

FVector AVoxelModule::VoxelIndexToLocation(FIndex InIndex)
{
	return InIndex.ToVector() * WorldData->BlockSize;
}

int32 AVoxelModule::GetChunkNum(bool bNeedGenerated /*= false*/) const
{
	int32 tmpNum = 0;
	for(auto Iter = ChunkMap.CreateConstIterator(); Iter; ++Iter)
	{
		if(AVoxelChunk* Chunk = Iter->Value)
		{
			if(!bNeedGenerated || Chunk->IsGenerated())
			{
				tmpNum++;
			}
		}
	}
	return tmpNum;
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

bool AVoxelModule::IsChunkGenerated(FIndex InIndex, bool bCheckVerticals)
{
	if(bCheckVerticals)
	{
		for(auto Iter : GetVerticalChunks(InIndex))
		{
			if(!Iter->IsGenerated())
			{
				return false;
			}
		}
		return true;
	}
	return FindChunkByIndex(InIndex) && FindChunkByIndex(InIndex)->IsGenerated();
}

TArray<AVoxelChunk*> AVoxelModule::GetVerticalChunks(FIndex InIndex)
{
	TArray<AVoxelChunk*> ReturnValue;
	for(int32 i = 0; i < WorldData->ChunkHeightRange; i++)
	{
		const FIndex Index = FIndex(InIndex.X, InIndex.Y, i);
		if(AVoxelChunk* Chunk = FindChunkByIndex(Index))
		{
			ReturnValue.Add(Chunk);
		}
	}
	return ReturnValue;
}

ECollisionChannel AVoxelModule::GetChunkTraceType() const
{
	return ECollisionChannel::ECC_MAX;
}

ECollisionChannel AVoxelModule::GetVoxelTraceType() const
{
	return ECollisionChannel::ECC_MAX;
}

EVoxelType AVoxelModule::GetNoiseVoxelType(FIndex InIndex)
{
	const FRandomStream& randomStream = WorldData->RandomStream;

	const int32 worldHeight = WorldData->GetWorldHeight();
	const FVector2D worldLocation = FVector2D(InIndex.X, InIndex.Y);

	const int32 plainHeight = UMathBPLibrary::GetNoiseHeight(worldLocation, WorldData->TerrainPlainScale, WorldData->WorldSeed, true) * worldHeight;
	const int32 mountainHeight = UMathBPLibrary::GetNoiseHeight(worldLocation, WorldData->TerrainMountainScale, WorldData->WorldSeed, true) * worldHeight;

	const int32 baseHeight = FMath::Max(plainHeight, mountainHeight) + WorldData->TerrainBaseHeight * worldHeight;

	if(InIndex.Z < baseHeight)
	{
		if(InIndex.Z <= WorldData->TerrainBedrockVoxelHeight * worldHeight)
		{
			return EVoxelType::Bedrock; //Bedrock
		}
		else if(InIndex.Z <= UMathBPLibrary::GetNoiseHeight(worldLocation, WorldData->TerrainStoneVoxelScale, WorldData->WorldSeed, true) * worldHeight)
		{
			return EVoxelType::Stone; //Stone
		}
		return EVoxelType::Dirt; //Dirt
	}
	else
	{
		const int32 waterHeight = WorldData->TerrainWaterVoxelHeight * worldHeight;
		if(InIndex.Z <= UMathBPLibrary::GetNoiseHeight(worldLocation, WorldData->TerrainSandVoxelScale, WorldData->WorldSeed, true) * worldHeight)
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
		else if(InIndex.Z == baseHeight + 1 && InIndex.Z != waterHeight + 1)
		{
			if(InIndex.Z <= UMathBPLibrary::GetNoiseHeight(worldLocation, WorldData->TerrainTreeVoxelScale, WorldData->WorldSeed, true) * worldHeight)
			{
				return randomStream.FRand() < 0.7f ? EVoxelType::Oak : EVoxelType::Birch; //Tree
			}
			else if(InIndex.Z <= UMathBPLibrary::GetNoiseHeight(worldLocation, WorldData->TerrainPlantVoxelScale, WorldData->WorldSeed, true) * worldHeight)
			{
				return randomStream.FRand() > 0.2f ? EVoxelType::Tall_Grass : (EVoxelType)randomStream.RandRange((int32)EVoxelType::Flower_Allium, (int32)EVoxelType::Flower_Tulip_White); //Plant
			}
		}
	}
	return EVoxelType::Empty; //Empty
}
