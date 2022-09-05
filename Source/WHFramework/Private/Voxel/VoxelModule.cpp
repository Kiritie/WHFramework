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

FVoxelWorldSaveData* AVoxelModule::WorldData = nullptr;

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

	WorldMode = EVoxelWorldMode::None;
	WorldState = EVoxelWorldState::None;

	ChunkSpawnClass = AVoxelChunk::StaticClass();
	
	ChunkSpawnRange = 7;
	ChunkBasicSpawnRange = 1;
	ChunkSpawnDistance = 3;

	ChunkSpawnSpeed = 100;
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

	GenerateVoxels();

	if(WorldMode != EVoxelWorldMode::None)
	{
		static FVoxelWorldSaveData SaveData;
		SaveData = FVoxelWorldSaveData(WorldBasicData);
		LoadSaveData(&SaveData, true);
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

FVoxelWorldSaveData& AVoxelModule::GetWorldData()
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
	USceneModuleBPLibrary::GetWorldTimer()->SetTimeSeconds(WorldData->TimeSeconds);

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
	WorldData->TimeSeconds = USceneModuleBPLibrary::GetWorldTimer()->GetTimeSeconds();
	return WorldData;
}

void AVoxelModule::UnloadData(bool bForceMode)
{
	if(bForceMode)
	{
		SetWorldState(EVoxelWorldState::None);

		for(auto iter : ChunkMap)
		{
			if(iter.Value)
			{
				UObjectPoolModuleBPLibrary::DespawnObject(iter.Value);
			}
		}
		ChunkMap.Empty();
		
		StopChunkTasks(ChunkMapLoadTasks);
		StopChunkTasks(ChunkMapBuildTasks);
		StopChunkTasks(ChunkMeshBuildTasks);

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
	if(WorldState != EVoxelWorldState::Spawning && (LastGenerateIndex == FIndex(-1, -1, -1) || FIndex::Distance(ChunkIndex, LastGenerateIndex, true) >= ChunkSpawnDistance))
	{
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
	VoxelsCapture->OrthoWidth = WorldBasicData.BlockSize * 4.f;
	
	int32 tmpIndex = 0;
	auto VoxelDatas = UAssetModuleBPLibrary::LoadPrimaryAssets<UVoxelData>(UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Voxel));
	for(int32 i = 0; i < VoxelDatas.Num(); i++)
	{
		if(VoxelDatas[i]->IsUnknown() || VoxelDatas[i]->IsCustom() || VoxelDatas[i]->PartType != EVoxelPartType::Main) continue;
		
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
	if (!ChunkMap.Contains(InIndex) || !RemoveFromMapLoadQueue(InIndex)) return;
	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);

	Chunk->LoadSaveData(WorldData->GetChunkData(InIndex));

	AddToMeshBuildQueue(InIndex);
}

void AVoxelModule::BuildChunkMap(FIndex InIndex, int32 InStage)
{
	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);
	if(!Chunk || !RemoveFromMapBuildQueue(InIndex, InStage)) return;

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
	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);
	if(!Chunk || !RemoveFromMeshBuildQueue(InIndex)) return;

	Chunk->BuildMesh();

	AddToGenerateQueue(InIndex);
}

void AVoxelModule::GenerateChunk(FIndex InIndex)
{
	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);
	if(!Chunk || !RemoveFromGenerateQueue(InIndex)) return;

	Chunk->Generate(false, WorldMode == EVoxelWorldMode::Normal);
}

void AVoxelModule::DestroyChunk(FIndex InIndex)
{
	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);
	if(!Chunk || !RemoveFromDestroyQueue(InIndex)) return;
	
	RemoveFromSpawnQueue(InIndex);
	RemoveFromMapLoadQueue(InIndex);
	RemoveFromMapBuildQueue(InIndex, 0);
	RemoveFromMapBuildQueue(InIndex, 1);
	RemoveFromMeshBuildQueue(InIndex);
	RemoveFromGenerateQueue(InIndex);
	
	WorldData->SetChunkData(InIndex, Chunk->ToSaveData<FVoxelChunkSaveData>());
	UObjectPoolModuleBPLibrary::DespawnObject(Chunk);

	ChunkMap.Remove(InIndex);
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

bool AVoxelModule::AddToGenerateQueue(FIndex InIndex)
{
	if(!ChunkMap.Contains(InIndex)) return false;

	ChunkGenerateQueue.AddUnique(InIndex);
	ChunkGenerateQueue.Sort([this](const FIndex& A, const FIndex& B){
		const float lengthA = FIndex::Distance(LastGenerateIndex, A, true);
		const float lengthB = FIndex::Distance(LastGenerateIndex, B, true);
		if(IsChunkGenerated(A) != IsChunkGenerated(B)) return !IsChunkGenerated(A);
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
	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);
	if(Chunk) return Chunk;

	RemoveFromSpawnQueue(InIndex);
	
	Chunk = UObjectPoolModuleBPLibrary::SpawnObject<AVoxelChunk>(nullptr, ChunkSpawnClass);
	if(Chunk)
	{
		ChunkMap.Add(InIndex, Chunk);

		Chunk->SetActorLocationAndRotation(InIndex.ToVector() * WorldData->GetChunkLength(), FRotator::ZeroRotator);
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
			for(auto Iter : Chunk->GetNeighbors())
			{
				if(Iter.Value && Iter.Value->GetBatch() != Chunk->GetBatch())
				{
					AddToMeshBuildQueue(Iter.Value->GetIndex());
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
		for(int32 i = 0; i < WorldData->ChunkHeightRange; i++)
		{
			const FIndex Index = FIndex(InIndex.X, InIndex.Y, i);
			if(!FindChunkByIndex(Index) || !FindChunkByIndex(Index)->IsGenerated())
			{
				return false;
			}
		}
		return true;
	}
	return FindChunkByIndex(InIndex) && FindChunkByIndex(InIndex)->IsGenerated();
}

ECollisionChannel AVoxelModule::GetChunkTraceType() const
{
	return ECollisionChannel::ECC_MAX;
}

ECollisionChannel AVoxelModule::GetVoxelTraceType() const
{
	return ECollisionChannel::ECC_MAX;
}
