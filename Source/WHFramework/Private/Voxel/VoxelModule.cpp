// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModule.h"

#include "Asset/AssetModuleStatics.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Voxel/EventHandle_VoxelWorldModeChanged.h"
#include "Event/Handle/Voxel/EventHandle_VoxelWorldStateChanged.h"
#include "Main/MainModuleStatics.h"
#include "Math/MathStatics.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "ReferencePool/ReferencePoolModuleStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelDoor.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/VoxelTorch.h"
#include "Voxel/Voxels/VoxelWater.h"
#include "Voxel/Voxels/Entity/VoxelEntityPreview.h"
#include "Common/CommonStatics.h"
#include "Common/CommonTypes.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Main/MainModule.h"
#include "Math/MathTypes.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/VoxelSaveGame.h"
#include "Scene/SceneModule.h"
#include "Voxel/Capture/VoxelCapture.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Voxels/VoxelTree.h"

IMPLEMENTATION_MODULE(UVoxelModule)

// Sets default values
UVoxelModule::UVoxelModule()
{
	ModuleName = FName("VoxelModule");
	ModuleDisplayName = FText::FromString(TEXT("Voxel Module"));
	ModuleSaveGame = UVoxelSaveGame::StaticClass();

	ModuleDependencies = { FName("AbilityModule"), FName("AudioModule"), FName("SceneModule") };

	VoxelCapture = nullptr;
	
	bAutoGenerate = false;
	WorldMode = EVoxelWorldMode::None;
	WorldState = EVoxelWorldState::None;
	WorldBasicData = FVoxelWorldBasicSaveData();

	WorldData = nullptr;

	ChunkSpawnClass = AVoxelChunk::StaticClass();
	
	ChunkSpawnDistance = 2;
	ChunkQueues = {
		{ EVoxelWorldState::Spawn, TArray{ FVoxelChunkQueue(false, 100) } },
		{ EVoxelWorldState::Destroy, TArray{ FVoxelChunkQueue(false, 10) } },
		{ EVoxelWorldState::Generate, TArray{ FVoxelChunkQueue(false, 1) } },
		{ EVoxelWorldState::MapLoad, TArray{ FVoxelChunkQueue(true, 100) } },
		{ EVoxelWorldState::MapBuild, TArray{ FVoxelChunkQueue(true, 100), FVoxelChunkQueue(false, 100) } },
		{ EVoxelWorldState::MeshBuild, TArray{ FVoxelChunkQueue(true, 100) } },
	};

	ChunkSpawnBatch = 0;
	ChunkGenerateIndex = EMPTY_Index;
	ChunkMap = TMap<FIndex, AVoxelChunk*>();

	VoxelClasses.Add(UVoxel::StaticClass());
	VoxelClasses.Add(UVoxelEmpty::StaticClass());
	VoxelClasses.Add(UVoxelUnknown::StaticClass());
	VoxelClasses.Add(UVoxelDoor::StaticClass());
	VoxelClasses.Add(UVoxelPlant::StaticClass());
	VoxelClasses.Add(UVoxelTorch::StaticClass());
	VoxelClasses.Add(UVoxelTree::StaticClass());
	VoxelClasses.Add(UVoxelWater::StaticClass());

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SolidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_Solid.M_Voxels_Solid'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitSolidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_Solid_Unlit.M_Voxels_Solid_Unlit'"));
	if(SolidMatFinder.Succeeded() && UnlitSolidMatFinder.Succeeded())
	{
		WorldBasicData.RenderDatas.Add(EVoxelTransparency::Solid, FVoxelRenderData(SolidMatFinder.Object, UnlitSolidMatFinder.Object));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SemiTransparentMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_SemiTransparent.M_Voxels_SemiTransparent'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitSemiTransparentMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_SemiTransparent_Unlit.M_Voxels_SemiTransparent_Unlit'"));
	if(SemiTransparentMatFinder.Succeeded() && UnlitSemiTransparentMatFinder.Succeeded())
	{
		WorldBasicData.RenderDatas.Add(EVoxelTransparency::SemiTransparent, FVoxelRenderData(SemiTransparentMatFinder.Object, UnlitSemiTransparentMatFinder.Object));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TransparentMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_Transparent.M_Voxels_Transparent'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitTransparentMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxels_Transparent_Unlit.M_Voxels_Transparent_Unlit'"));
	if(TransparentMatFinder.Succeeded() && UnlitTransparentMatFinder.Succeeded())
	{
		WorldBasicData.RenderDatas.Add(EVoxelTransparency::Transparent, FVoxelRenderData(TransparentMatFinder.Object, UnlitTransparentMatFinder.Object));
	}
		
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IconSourceMatFinder(TEXT("/Script/Engine.Material'/WHFramework/Voxel/Materials/M_Voxels_Icon.M_Voxels_Icon'"));
	if(IconSourceMatFinder.Succeeded())
	{
		WorldBasicData.IconMat = IconSourceMatFinder.Object;
	}
}

UVoxelModule::~UVoxelModule()
{
	TERMINATION_MODULE(UVoxelModule)
}

#if WITH_EDITOR
void UVoxelModule::OnGenerate()
{
	if(!VoxelCapture)
	{
		TArray<AActor*> ChildActors;
		GetModuleOwner()->GetAttachedActors(ChildActors);
		if(ChildActors.Num() > 0)
		{
			VoxelCapture = Cast<AVoxelCapture>(ChildActors[0]);
		}
	}
	if(!VoxelCapture)
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		VoxelCapture = GetWorld()->SpawnActor<AVoxelCapture>(ActorSpawnParameters);
		if(VoxelCapture)
		{
			VoxelCapture->SetActorLabel(TEXT("VoxelCapture"));
			VoxelCapture->AttachToActor(GetModuleOwner(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	if(VoxelCapture)
	{
		VoxelCapture->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	Modify();
}

void UVoxelModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UVoxelModule)

	if(VoxelCapture)
	{
		VoxelCapture->Destroy();
	}
}
#endif

void UVoxelModule::OnInitialize()
{
	Super::OnInitialize();

	UAssetModuleStatics::AddStaticObject(FName("EVoxelType"), FStaticObject(UEnum::StaticClass(), TEXT("/Script/WHFramework.EVoxelType")));

	UAssetModuleStatics::AddEnumMapping(TEXT("/Script/WHFramework.EInteractAction"), TEXT("/Script/WHFramework.EVoxelInteractAction"));

	for(const auto Iter1 : UAssetModuleStatics::LoadPrimaryAssets<UVoxelData>(FName("Voxel")))
	{
		for(auto& Iter2 : Iter1->MeshDatas)
		{
			for(auto& Iter3 : Iter2.MeshUVDatas)
			{
				if(Iter3.Texture && WorldBasicData.RenderDatas.Contains(Iter1->Transparency))
				{
					Iter3.UVOffset = FVector2D(0.f, WorldBasicData.RenderDatas[Iter1->Transparency].Textures.AddUnique(Iter3.Texture));
				}
			}
		}
	}
	for(auto& Iter : WorldBasicData.RenderDatas)
	{
		Iter.Value.TextureSize = FVector2D(Iter.Value.PixelSize, Iter.Value.Textures.Num() * Iter.Value.PixelSize);

		if(UTexture2D* Texture = UCommonStatics::CompositeTextures(Iter.Value.Textures, Iter.Value.TextureSize))
		{
			Iter.Value.CombineTexture = Texture;
				
			UMaterialInstanceDynamic* MatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, Iter.Value.Material);
			MatInst->SetTextureParameterValue(FName("Texture"), Texture);
			Iter.Value.MaterialInst = MatInst;
			
			MatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, Iter.Value.UnlitMaterial);
			MatInst->SetTextureParameterValue(FName("Texture"), Texture);
			Iter.Value.UnlitMaterialInst = MatInst;
		}
	}
	for(const auto& Iter : VoxelClasses)
	{
		UReferencePoolModuleStatics::CreateReference(nullptr, Iter);
	}
}

void UVoxelModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UVoxelModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor) return;

	if(WorldMode != EVoxelWorldMode::None)
	{
		GenerateChunkQueues();
		GenerateWorld();
	}
}

void UVoxelModule::OnPause()
{
	Super::OnPause();
}

void UVoxelModule::OnUnPause()
{
	Super::OnUnPause();
}

void UVoxelModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		DestroyChunkQueues();
	}
}

void UVoxelModule::Load_Implementation()
{
	if(bModuleAutoSave)
	{
		USaveGameModuleStatics::LoadOrCreateSaveGame(ModuleSaveGame, 0, bAutoGenerate ? EPhase::All : EPhase::Primary);
	}
	else if(!WorldData)
	{
		LoadSaveData(NewWorldData(), bAutoGenerate ? EPhase::All : EPhase::Primary);
	}
}

void UVoxelModule::Save_Implementation()
{
	Super::Save_Implementation();
}

FString UVoxelModule::GetModuleDebugMessage()
{
	return Super::GetModuleDebugMessage();
}

void UVoxelModule::SetWorldMode(EVoxelWorldMode InWorldMode)
{
	if(WorldMode != InWorldMode)
	{
		WorldMode = InWorldMode;
		OnWorldModeChanged();
	}
}

void UVoxelModule::SetWorldState(EVoxelWorldState InWorldState)
{
	if(WorldState != InWorldState)
	{
		WorldState = InWorldState;
		OnWorldStateChanged();
	}
}

void UVoxelModule::OnWorldModeChanged()
{
	UEventModuleStatics::BroadcastEvent(UEventHandle_VoxelWorldModeChanged::StaticClass(), this, {&WorldMode});
}

void UVoxelModule::OnWorldStateChanged()
{
	UEventModuleStatics::BroadcastEvent(UEventHandle_VoxelWorldStateChanged::StaticClass(), this, {&WorldState});
}

FVoxelWorldSaveData& UVoxelModule::GetWorldData() const
{
	return WorldData ? *WorldData : FVoxelWorldSaveData::Empty;
}

FVoxelWorldSaveData* UVoxelModule::NewWorldData(FSaveData* InBasicData) const
{
	static FVoxelModuleSaveData SaveData;
	SaveData = !InBasicData ? FVoxelModuleSaveData(WorldBasicData) : InBasicData->CastRef<FVoxelModuleSaveData>();
	return &SaveData;
}

void UVoxelModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FVoxelWorldSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		WorldData = NewWorldData(InSaveData);

		WorldData->RenderDatas = WorldBasicData.RenderDatas;
		
		if(WorldData->WorldSeed == 0)
		{
			WorldData->WorldSeed = FMath::Rand();
		}
		WorldData->RandomStream = FRandomStream(WorldData->WorldSeed);

		VoxelCapture->GetCapture()->SetActive(true);
		VoxelCapture->GetCapture()->OrthoWidth = WorldData->BlockSize * 4.f;
		
		int32 ItemIndex = 0;
		ITER_ARRAY(UAssetModuleStatics::LoadPrimaryAssets<UVoxelData>(FName("Voxel")), Item,
			if(Item->IsUnknown() || !Item->IsMainPart()) continue;
			
			AVoxelEntityPreview* VoxelEntity;
			if(PreviewVoxels.IsValidIndex(ItemIndex))
			{
				VoxelEntity = PreviewVoxels[ItemIndex];
			}
			else
			{
				VoxelEntity = UObjectPoolModuleStatics::SpawnObject<AVoxelEntityPreview>();
				VoxelCapture->GetCapture()->ShowOnlyActors.Add(VoxelEntity);
				PreviewVoxels.EmplaceAt(ItemIndex, VoxelEntity);
			}
			if(VoxelEntity)
			{
				VoxelEntity->LoadSaveData(new FVoxelItem(Item->GetPrimaryAssetId()));
				VoxelEntity->SetActorLocation(FVector((ItemIndex / 8 - 3.5f) * WorldBasicData.BlockSize * 0.5f, (ItemIndex % 8 - 3.5f) * WorldBasicData.BlockSize * 0.5f, -800.f));
				VoxelEntity->SetActorRotation(FRotator(-70.f, 0.f, -180.f));
				VoxelEntity->GetMeshComponent()->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
				VoxelEntity->GetMeshComponent()->SetRelativeScale3D(FVector(0.3f));

				if(!Item->Icon)
				{
					const auto IconMat = UMaterialInstanceDynamic::Create(WorldBasicData.IconMat, nullptr);
					IconMat->SetTextureParameterValue(FName("Texture"), VoxelCapture->GetCapture()->TextureTarget);
					IconMat->SetScalarParameterValue(FName("SizeX"), 8.f);
					IconMat->SetScalarParameterValue(FName("SizeY"), 8.f);
					IconMat->SetScalarParameterValue(FName("Index"), ItemIndex);
					Item->Icon = IconMat;
				}
			}
			ItemIndex++;
		)
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		SetWorldMode(EVoxelWorldMode::Preview);
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		SetWorldMode(EVoxelWorldMode::Default);
	}
	if(PHASEC(InPhase, EPhase::All))
	{
		if(SaveData.SceneData.WeatherData.WeatherSeed == 0)
		{
			SaveData.SceneData.WeatherData.WeatherSeed = WorldData->WorldSeed;
		}
		USceneModule::Get().LoadSaveData(&SaveData.SceneData, InPhase);
	}
}

FSaveData* UVoxelModule::ToData()
{
	static FVoxelWorldSaveData* SaveData;
	SaveData = NewWorldData(WorldData);
	
	for(auto& Iter : ChunkMap)
	{
		if(Iter.Value->IsGenerated())
		{
			SaveData->SetChunkData(Iter.Key, Iter.Value->GetSaveData<FVoxelChunkSaveData>(true));
		}
	}

	SaveData->SceneData = USceneModule::Get().GetSaveDataRef<FSceneModuleSaveData>(true);
	
	return SaveData;
}

void UVoxelModule::UnloadData(EPhase InPhase)
{
	if(PHASEC(InPhase, EPhase::Primary))
	{
		SetWorldMode(EVoxelWorldMode::None);
		SetWorldState(EVoxelWorldState::None);

		DestroyChunkQueues();

		for(auto& Iter : ChunkMap)
		{
			UObjectPoolModuleStatics::DespawnObject(Iter.Value);
		}
		
		ChunkMap.Empty();

		ChunkSpawnBatch = 0;
		ChunkGenerateIndex = EMPTY_Index;

		WorldData = NewWorldData();

		VoxelCapture->GetCapture()->SetActive(false);
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		SetWorldMode(EVoxelWorldMode::Preview);
	}
}

void UVoxelModule::GenerateWorld()
{
	// Destroy chunk
	if(UpdateChunkQueue(EVoxelWorldState::Destroy, [this](FIndex Index, int32 Stage){ DestroyChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Destroy);
	}
	// Spawn chunk
	else if(UpdateChunkQueue(EVoxelWorldState::Spawn, [this](FIndex Index, int32 Stage){ SpawnChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Spawn);
	}
	// Load chunk map
	else if(UpdateChunkQueue(EVoxelWorldState::MapLoad, [this](FIndex Index, int32 Stage){ LoadChunkMap(Index); }))
	{
		SetWorldState(EVoxelWorldState::MapLoad);
	}
	// Build chunk map
	else if(UpdateChunkQueue(EVoxelWorldState::MapBuild, [this](FIndex Index, int32 Stage){ BuildChunkMap(Index, Stage); }))
	{
		SetWorldState(EVoxelWorldState::MapBuild);
	}
	// Build chunk mesh
	else if(UpdateChunkQueue(EVoxelWorldState::MeshBuild, [this](FIndex Index, int32 Stage){ BuildChunkMesh(Index); }))
	{
		SetWorldState(EVoxelWorldState::MeshBuild);
	}
	// Generate chunk
	else if(UpdateChunkQueue(EVoxelWorldState::Generate, [this](FIndex Index, int32 Stage){ GenerateChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Generate);
	}
}

void UVoxelModule::LoadChunkMap(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		Chunk->LoadSaveData(WorldData->GetChunkData(InIndex));
	}
}

void UVoxelModule::BuildChunkMap(FIndex InIndex, int32 InStage)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		Chunk->BuildMap(InStage);
	}
}

void UVoxelModule::BuildChunkMesh(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		Chunk->BuildMesh();
	}
}

AVoxelChunk* UVoxelModule::SpawnChunk(FIndex InIndex, bool bAddToQueue)
{
	AVoxelChunk* Chunk = FindChunkByIndex(InIndex);
	if(!Chunk)
	{
		Chunk = UObjectPoolModuleStatics::SpawnObject<AVoxelChunk>(nullptr, nullptr, false, ChunkSpawnClass);
		Chunk->Initialize(InIndex, ChunkSpawnBatch + (IsOnTheWorld(InIndex) ? 0 : 1));
		Chunk->SetActorLocationAndRotation(InIndex.ToVector() * WorldData->GetChunkRealSize(), FRotator::ZeroRotator);
		ChunkMap.Add(InIndex, Chunk);
	}
	if(bAddToQueue)
	{
		if(!Chunk->IsBuilded())
		{
			if(IsOnTheWorld(InIndex, false))
			{
				if(WorldData->IsChunkHasVoxelData(InIndex))
				{
					AddToChunkQueue(EVoxelWorldState::MapLoad, InIndex);
				}
				else
				{
					AddToChunkQueue(EVoxelWorldState::MapBuild, InIndex);
				}
			}
			else if(IsOnTheWorld(InIndex))
			{
				Chunk->bBuilded = true;
			}
		}
		if(!Chunk->IsGenerated() && IsOnTheWorld(InIndex))
		{
			AddToChunkQueue(EVoxelWorldState::MeshBuild, InIndex);
			AddToChunkQueue(EVoxelWorldState::Generate, InIndex);
			for(const auto& Iter : Chunk->GetNeighbors())
			{
				if(Iter.Value && Iter.Value->GetBatch() != Chunk->GetBatch())
				{
					AddToChunkQueue(EVoxelWorldState::MeshBuild, Iter.Value->GetIndex());
					AddToChunkQueue(EVoxelWorldState::Generate, Iter.Value->GetIndex());
				}
			}
		}
	}
	return Chunk;
}

void UVoxelModule:: GenerateChunk(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		Chunk->Generate(EPhase::Primary);
	}
}

void UVoxelModule::DestroyChunk(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = FindChunkByIndex(InIndex))
	{
		UObjectPoolModuleStatics::DespawnObject(Chunk);
		ChunkMap.Remove(InIndex);
	}
}

void UVoxelModule::GenerateChunkQueues(bool bFromAgent, bool bForce)
{
	if(bForce) DestroyChunkQueues();
	FIndex GenerateIndex = FIndex::ZeroIndex;
	FVector GenerateOffset = FVector::ZeroVector;
	const auto VoxelAgent  = Cast<IVoxelAgentInterface>(UCommonStatics::GetPlayerController()->GetViewTarget());
	if(bFromAgent && VoxelAgent)
	{
		const FVector AgentLocation = FVector(WorldData->WorldRange.X != 0.f ? VoxelAgent->GetVoxelAgentLocation().X : 0.f, WorldData->WorldRange.Y != 0.f ? VoxelAgent->GetVoxelAgentLocation().Y : 0.f, 0.f);
		GenerateIndex = LocationToChunkIndex(AgentLocation);
		GenerateOffset = (AgentLocation / WorldData->GetChunkRealSize() - ChunkGenerateIndex.ToVector()).GetAbs();
	}
	if(bForce || ChunkGenerateIndex == EMPTY_Index || (WorldData->WorldRange.X != 0.f && GenerateOffset.X > FMath::Min(ChunkSpawnDistance, WorldData->GetWorldSize().X * 0.5f) || WorldData->WorldRange.Y != 0.f && GenerateOffset.Y > FMath::Min(ChunkSpawnDistance, WorldData->GetWorldSize().Y * 0.5f)))
	{
		TArray<FIndex> DestroyQueue;
		ChunkMap.GenerateKeyArray(DestroyQueue);
		const FVector SpawnRange = FVector(WorldData->GetWorldSize().X * 0.5f, WorldData->GetWorldSize().Y * 0.5f, WorldData->GetWorldSize().Z);
		for(int32 x = GenerateIndex.X - SpawnRange.X; x < GenerateIndex.X + SpawnRange.X; x++)
		{
			for(int32 y = GenerateIndex.Y - SpawnRange.Y; y < GenerateIndex.Y + SpawnRange.Y; y++)
			{
				for(int32 z = 0; z < SpawnRange.Z; z++)
				{
					const FIndex Index = FIndex(x, y, z);
					if(DestroyQueue.Contains(Index))
					{
						DestroyQueue.Remove(Index);
					}
					AddToChunkQueue(EVoxelWorldState::Spawn, Index);
				}
			}
		}
		ITER_ARRAY(DestroyQueue, Item, AddToChunkQueue(EVoxelWorldState::Destroy, Item););
		ChunkGenerateIndex = GenerateIndex;
		ChunkSpawnBatch++;
	}
}

void UVoxelModule::DestroyChunkQueues()
{
	ITER_MAP(ChunkQueues, Item1,
		ITER_ARRAY(Item1.Value.Queues, Item2,
			Item2.Queue.Empty();
			ITER_ARRAY(Item2.Tasks, Item3,
				Item3->EnsureCompletion();
				delete Item3;
			)
			Item2.Tasks.Empty();
		)
	)
}

bool UVoxelModule::UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex, int32)> InFunc)
{
	ITER_ARRAY_WITHINDEX(ChunkQueues[InState].Queues, i, Item,
		if(Item.bAsync)
		{
			if(Item.Tasks.Num() == 0 && Item.Queue.Num() > 0)
			{
				DON_WITHINDEX(FMath::CeilToInt((float)Item.Queue.Num() / Item.Speed), j,
					TArray<FIndex> Queue;
					DON_WITHINDEX(FMath::Min(Item.Speed, Item.Queue.Num() - j * Item.Speed), k,
						Queue.Add(Item.Queue[j * Item.Speed + k]);
					)
					const auto Task = new FAsyncTask<AsyncTask_ChunkQueue>(Queue, InFunc, i);
					Item.Tasks.Add(Task);
					Task->StartBackgroundTask();
				)
			}
			else if(Item.Tasks.Num() > 0)
			{
				const auto Tasks = Item.Tasks;
				ITER_ARRAY(Tasks, Task,
					if(Task->IsDone())
					{
						ITER_ARRAY(Task->GetTask().GetChunkQueue(), Index,
							Item.Queue.Remove(Index);
						)
						Item.Tasks.Remove(Task);
						delete Task;
					}
				)
			}
			if(Item.Queue.Num() > 0 || Item.Tasks.Num() > 0)
			{
				return true;
			}
		}
		else
		{
			const int32 Num = FMath::Min(Item.Speed, Item.Queue.Num());
			DON_WITHINDEX(Num, j, InFunc(Item.Queue[j], i); )
			Item.Queue.RemoveAt(0, Num);
			if(Item.Queue.Num() > 0)
			{
				return true;
			}
		}
	)
	return false;
}

void UVoxelModule::AddToChunkQueue(EVoxelWorldState InState, FIndex InIndex)
{
	ITER_ARRAY(ChunkQueues[InState].Queues, Item,
		if(!Item.Queue.Contains(InIndex))
		{
			Item.Queue.Add(InIndex);
			switch(InState)
			{
				case EVoxelWorldState::Spawn:
				{
					RemoveFromChunkQueue(EVoxelWorldState::Destroy, InIndex);
					break;
				}
				case EVoxelWorldState::Generate:
				{
					Item.Queue.Sort([this](const FIndex& A, const FIndex& B){
						const float LengthA = ChunkGenerateIndex.DistanceTo(A, false, true);
						const float LengthB = ChunkGenerateIndex.DistanceTo(B, false, true);
						return LengthA < LengthB;
					});
					break;
				}
				case EVoxelWorldState::Destroy:
				{
					Item.Queue.Sort([this](const FIndex& A, const FIndex& B){
						const float LengthA = ChunkGenerateIndex.DistanceTo(A, false, true);
						const float LengthB = ChunkGenerateIndex.DistanceTo(B, false, true);
						return LengthA > LengthB;
					});
					break;
				}
				default: break;
			}
		}
	)
}

void UVoxelModule::RemoveFromChunkQueue(EVoxelWorldState InState, FIndex InIndex)
{
	ITER_ARRAY(ChunkQueues[InState].Queues, Item,
		if(Item.Queue.Contains(InIndex))
		{
			Item.Queue.Remove(InIndex);
		}
	)
}

bool UVoxelModule::IsOnTheWorld(FIndex InIndex, bool bIgnoreZ) const
{
	const FVector SpawnRange = FVector(WorldData->GetWorldSize().X * 0.5f, WorldData->GetWorldSize().Y * 0.5f, WorldData->GetWorldSize().Z);
	return InIndex.X >= ChunkGenerateIndex.X - SpawnRange.X && InIndex.X < ChunkGenerateIndex.X + SpawnRange.X &&
		InIndex.Y >= ChunkGenerateIndex.Y - SpawnRange.Y && InIndex.Y < ChunkGenerateIndex.Y + SpawnRange.Y &&
		(!bIgnoreZ || InIndex.Z >= ChunkGenerateIndex.Z && InIndex.Z < ChunkGenerateIndex.Z + SpawnRange.Z);
}

AVoxelChunk* UVoxelModule::FindChunkByIndex(FIndex InIndex) const
{
	if(ChunkMap.Contains(InIndex))
	{
		return ChunkMap[InIndex];
	}
	return nullptr;
}

AVoxelChunk* UVoxelModule::FindChunkByLocation(FVector InLocation) const
{
	return FindChunkByIndex(LocationToChunkIndex(InLocation));
}

FVoxelItem& UVoxelModule::FindVoxelByIndex(FIndex InIndex)
{
	return FindVoxelByLocation(VoxelIndexToLocation(InIndex));
}

FVoxelItem& UVoxelModule::FindVoxelByLocation(FVector InLocation)
{
	if(AVoxelChunk* Chunk = FindChunkByLocation(InLocation))
	{
		return Chunk->GetVoxelItem(Chunk->LocationToIndex(InLocation));
	}
	return FVoxelItem::Empty;
}

float UVoxelModule::GetNoiseHeight(FVector2D InLocation, FVector InScale, int32 InOffset) const
{
	return UMathStatics::GetNoiseHeight(InLocation, InScale, InOffset, true) * WorldData->ChunkSize.Z * WorldData->WorldSize.Z;
}

float UVoxelModule::GetNoiseHeight(float InBaseHeight) const
{
	return InBaseHeight * WorldData->ChunkSize.Z * WorldData->WorldSize.Z;
}

EVoxelType UVoxelModule::GetNoiseVoxelType(FIndex InIndex) const
{
	return GetNoiseVoxelType(InIndex.X, InIndex.Y, InIndex.Z);
}

EVoxelType UVoxelModule::GetNoiseVoxelType(int32 InX, int32 InY, int32 InZ) const
{
	const FVector2D WorldLocation = FVector2D(InX, InY);

	const int32 PlainHeight = GetNoiseHeight(WorldLocation, WorldData->PlainScale, WorldData->WorldSeed);
	const int32 MountainHeight = GetNoiseHeight(WorldLocation, WorldData->MountainScale, WorldData->WorldSeed);
	const int32 BlobHeight = GetNoiseHeight(WorldLocation, WorldData->BlobScale, WorldData->WorldSeed);

	const int32 BaseHeight = GetNoiseHeight(WorldData->BaseHeight) + FMath::Max3(PlainHeight, MountainHeight, BlobHeight);

	if(InZ < BaseHeight)
	{
		if(InZ <= GetNoiseHeight(WorldData->BedrockHeight))
		{
			return EVoxelType::Bedrock; //Bedrock
		}
		if(InZ <= GetNoiseHeight(WorldLocation, WorldData->StoneScale, WorldData->WorldSeed))
		{
			return EVoxelType::Stone; //Stone
		}
		return EVoxelType::Dirt; //Dirt
	}
	if(InZ >= BaseHeight)
	{
		const int32 SandHeight = GetNoiseHeight(WorldLocation, WorldData->SandScale, WorldData->WorldSeed);
		const int32 WaterHeight = GetNoiseHeight(WorldData->WaterHeight);
		if(InZ <= SandHeight)
		{
			return EVoxelType::Sand; //Sand
		}
		if(InZ <= WaterHeight)
		{
			return EVoxelType::Water; //Water
		}
		if(InZ == BaseHeight)
		{
			return EVoxelType::Grass; //Grass
		}
	}
	return EVoxelType::Empty;
}

EVoxelType UVoxelModule::GetRandomVoxelType(FIndex InIndex) const
{
	return GetRandomVoxelType(InIndex.X, InIndex.Y, InIndex.Z);
}

EVoxelType UVoxelModule::GetRandomVoxelType(int32 InX, int32 InY, int32 InZ) const
{
	const FVector2D WorldLocation = FVector2D(InX, InY);

	const FRandomStream& RandomStream = WorldData->RandomStream;
	if(InZ <= GetNoiseHeight(WorldLocation, WorldData->TreeScale, WorldData->WorldSeed) && RandomStream.FRand() <= WorldData->TreeScale.W)
	{
		return RandomStream.FRand() < 0.6f ? EVoxelType::Oak : EVoxelType::Birch; //Tree
	}
	if(InZ <= GetNoiseHeight(WorldLocation, WorldData->PlantScale, WorldData->WorldSeed) && RandomStream.FRand() <= WorldData->PlantScale.W)
	{
		return RandomStream.FRand() < 0.3f ? EVoxelType::Tall_Grass : (EVoxelType)RandomStream.RandRange((int32)EVoxelType::Flower_Allium, (int32)EVoxelType::Flower_Tulip_White); //Plant
	}
	return EVoxelType::Empty;
}

FIndex UVoxelModule::LocationToChunkIndex(FVector InLocation, bool bIgnoreZ /*= false*/) const
{
	InLocation /= WorldData->GetChunkRealSize();
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), bIgnoreZ ? 0 : FMath::FloorToInt(InLocation.Z));
}

FVector UVoxelModule::ChunkIndexToLocation(FIndex InIndex) const
{
	return InIndex.ToVector() * WorldData->GetChunkRealSize();
}

FIndex UVoxelModule::LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ) const
{
	InLocation /= WorldData->BlockSize;
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), bIgnoreZ ? 0 : FMath::FloorToInt(InLocation.Z));
}

FVector UVoxelModule::VoxelIndexToLocation(FIndex InIndex) const
{
	return InIndex.ToVector() * WorldData->BlockSize;
}

int32 UVoxelModule::VoxelIndexToNumber(FIndex InIndex) const
{
	const int32 Num = (int32)WorldData->ChunkSize.X;
	return InIndex.X + InIndex.Y * Num + InIndex.Z * Num * (int32)WorldData->ChunkSize.Y;
}

FIndex UVoxelModule::NumberToVoxelIndex(int32 InNumber) const
{
	const int32 Num1 = (int32)WorldData->ChunkSize.X;
	const int32 Num2 = Num1 * (int32)WorldData->ChunkSize.Y;
	const int32 Num3 = InNumber % Num2;
	return FIndex(Num3 % Num1, Num3 / Num1, InNumber / Num2);
}

bool UVoxelModule::VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	FHitResult HitResult;
	if(UKismetSystemLibrary::LineTraceSingle(GetWorldContext(), InRayStart, InRayEnd, USceneModuleStatics::GetTraceMapping(FName("Voxel")).GetTraceType(), false, InIgnoreActors, EDrawDebugTrace::None, HitResult, true))
	{
		OutHitResult = FVoxelHitResult(HitResult);
		return OutHitResult.IsValid();
	}
	return false;
}

bool UVoxelModule::VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	if(AWHPlayerController* PlayerController = UCommonStatics::GetPlayerController())
	{
		FHitResult HitResult;
		switch (InRaycastType)
		{
			case EVoxelRaycastType::FromAimPoint:
			{
				PlayerController->RaycastSingleFromViewportPosition(FVector2D(0.5f), InDistance, USceneModuleStatics::GetTraceMapping(FName("Voxel")).GetTraceChannel(), InIgnoreActors, HitResult);
				break;
			}
			case EVoxelRaycastType::FromMousePosition:
			{
				PlayerController->RaycastSingleFromMousePosition(InDistance, USceneModuleStatics::GetTraceMapping(FName("Voxel")).GetTraceChannel(), InIgnoreActors, HitResult);
				break;
			}
		}
		OutHitResult = FVoxelHitResult(HitResult);
		return OutHitResult.IsValid();
	}
	return false;
}

bool UVoxelModule::VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	const FVector Size = InVoxelItem.GetRange() * WorldData->BlockSize * 0.5f;
	const FVector Location = InVoxelItem.GetLocation();
	return UKismetSystemLibrary::BoxTraceSingle(GetWorldContext(), Location + Size, Location + Size, Size * 0.95f, FRotator::ZeroRotator, USceneModuleStatics::GetTraceMapping(FName("Voxel")).GetTraceType(), false, InIgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
}

bool UVoxelModule::VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter)
{
	const FVector ChunkRadius = WorldData->GetChunkRealSize() * 0.5f;
	const FVector ChunkLocation = ChunkIndexToLocation(InChunkIndex);
	return VoxelAgentTraceSingle(ChunkLocation + ChunkRadius, FVector2D(ChunkRadius.X, ChunkRadius.Y), InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter);
}

bool UVoxelModule::VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter)
{
	const FBox WorldBounds = GetWorldBounds(InRadius, InHalfHeight);
	InLocation.X = FMath::Clamp(InLocation.X, WorldBounds.Min.X, WorldBounds.Max.X);
	InLocation.Y = FMath::Clamp(InLocation.Y, WorldBounds.Min.Y, WorldBounds.Max.Y);
	DON_WITHINDEX(InMaxCount, i,
		FVector RayStart = FVector((bFromCenter && i == 0) ? 0.f : WorldData->RandomStream.FRandRange(-InRange.X * 0.5f, InRange.X * 0.5f),
			(bFromCenter && i == 0) ? 0.f : WorldData->RandomStream.FRandRange(-InRange.Y * 0.5f, InRange.Y * 0.5f), WorldData->GetWorldRealSize().Z);
		RayStart.X = FMath::Clamp(InLocation.X + (bSnapToBlock ? (FMath::Floor(RayStart.X / WorldData->BlockSize) + 0.5f) * WorldData->BlockSize : RayStart.X), WorldBounds.Min.X, WorldBounds.Max.X);
		RayStart.Y = FMath::Clamp(InLocation.Y + (bSnapToBlock ? (FMath::Floor(RayStart.Y / WorldData->BlockSize) + 0.5f) * WorldData->BlockSize : RayStart.Y), WorldBounds.Min.Y, WorldBounds.Max.Y);
		const FVector RayEnd = FVector(RayStart.X, RayStart.Y, 0.f);
		FHitResult HitResult;
		if(VoxelAgentTraceSingle(RayStart, RayEnd, InRadius, InHalfHeight, InIgnoreActors, HitResult))
		{
			OutHitResult = HitResult;
			return true;
		}
	)
	return false;
}

bool UVoxelModule::VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	FHitResult HitResult1;
	if(UKismetSystemLibrary::CapsuleTraceSingle(GetWorldContext(), InRayStart, InRayEnd, InRadius * 0.95f, InHalfHeight, USceneModuleStatics::GetTraceMapping(FName("Chunk")).GetTraceType(), false, InIgnoreActors, EDrawDebugTrace::None, HitResult1, true))
	{
		FHitResult HitResult2;
		if(!UKismetSystemLibrary::CapsuleTraceSingle(GetWorldContext(), HitResult1.Location, HitResult1.Location, InRadius * 0.95f, InHalfHeight * 0.95f, USceneModuleStatics::GetTraceMapping(FName("Voxel")).GetTraceType(), false, InIgnoreActors, EDrawDebugTrace::None, HitResult2, true))
		{
			FVoxelItem& VoxelItem = FindVoxelByLocation(HitResult1.Location);
			if(!VoxelItem.IsValid())
			{
				OutHitResult = HitResult1;
				return true;
			}
		}
	}
	return false;
}

bool UVoxelModule::IsBasicGenerated() const
{
	ITER_INDEX(Iter, FVector(FMath::Min(ChunkSpawnDistance, WorldData->GetWorldSize().X * 0.5), FMath::Min(ChunkSpawnDistance, WorldData->GetWorldSize().Y * 0.5), WorldData->GetWorldSize().Z), true,
		if(!IsChunkGenerated(Iter + ChunkGenerateIndex))
		{
			return false;
		}
	)
	return true;
}

FBox UVoxelModule::GetWorldBounds(float InRadius, float InHalfHeight) const
{
	const FVector WorldRadius = WorldData->GetWorldRealSize() * 0.5f;
	const FVector WorldCenter = FVector(ChunkIndexToLocation(ChunkGenerateIndex).X + ((int32)WorldData->GetWorldSize().X % 2 == 1 ? WorldData->GetChunkRealSize().X * 0.5f : 0.f), ChunkIndexToLocation(ChunkGenerateIndex).Y + ((int32)WorldData->GetWorldSize().Y % 2 == 1 ? WorldData->GetChunkRealSize().Y * 0.5f : 0.f), WorldRadius.Z);
	return FBox(WorldCenter - FVector(WorldRadius.X - InRadius, WorldRadius.Y - InRadius, WorldRadius.Z - InHalfHeight), WorldCenter + FVector(WorldRadius.X - InRadius, WorldRadius.Y - InRadius, WorldRadius.Z - InHalfHeight));
}

int32 UVoxelModule::GetChunkNum(bool bNeedGenerated /*= false*/) const
{
	if(bNeedGenerated)
	{
		int32 ReturnValue = 0;
		for(auto& Iter : ChunkMap)
		{
			if(Iter.Value->IsGenerated())
			{
				ReturnValue++;
			}
		}
		return ReturnValue;
	}
	return ChunkMap.Num();
}

bool UVoxelModule::IsChunkGenerated(FIndex InIndex, bool bCheckVerticals) const
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

TArray<AVoxelChunk*> UVoxelModule::GetVerticalChunks(FIndex InIndex) const
{
	TArray<AVoxelChunk*> ReturnValue;
	for(int32 i = 0; i < WorldData->WorldSize.Z; i++)
	{
		const FIndex Index = FIndex(InIndex.X, InIndex.Y, i);
		if(AVoxelChunk* Chunk = FindChunkByIndex(Index))
		{
			ReturnValue.Add(Chunk);
		}
	}
	return ReturnValue;
}
