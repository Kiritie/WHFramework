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
#include "Voxel/Voxels/Entity/VoxelEntityCapture.h"
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
#include "Voxel/Generators/VoxelBiomeGenerator.h"
#include "Voxel/Generators/VoxelBuildingGenerator.h"
#include "Voxel/Generators/VoxelCaveGenerator.h"
#include "Voxel/Generators/VoxelGenerator.h"
#include "Voxel/Generators/VoxelHeightGenerator.h"
#include "Voxel/Generators/VoxelHumidityGenerator.h"
#include "Voxel/Generators/VoxelFoliageGenerator.h"
#include "Voxel/Generators/VoxelOreGenerator.h"
#include "Voxel/Generators/VoxelRainGenerator.h"
#include "Voxel/Generators/VoxelTemperatureGenerator.h"
#include "Voxel/Generators/VoxelTerrainGenerator.h"
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
	
	ChunkSpawnDistance = 5;
	ChunkQueues = {
		{ EVoxelWorldState::Spawning, FVoxelChunkQueues({
			FVoxelChunkQueue(false, 100)
		})}, { EVoxelWorldState::Destroying, FVoxelChunkQueues({
			FVoxelChunkQueue(false, 10)
		})}, { EVoxelWorldState::Generating, FVoxelChunkQueues({
			FVoxelChunkQueue(false, 3)
		})}, { EVoxelWorldState::MapLoading, FVoxelChunkQueues({
			FVoxelChunkQueue(true, 100)
		})}, { EVoxelWorldState::MapBuilding, FVoxelChunkQueues({
			FVoxelChunkQueue(true, 100),
			FVoxelChunkQueue(false, 50)
		})}, { EVoxelWorldState::MeshSpawning, FVoxelChunkQueues({
			FVoxelChunkQueue(true, 100),
			FVoxelChunkQueue(false, 50)
		})}, { EVoxelWorldState::MeshBuilding, FVoxelChunkQueues({
			FVoxelChunkQueue(true, 100)
		})}
	};

	ChunkSpawnBatch = 0;
	ChunkGenerateIndex = EMPTY_Index;
	ChunkMap = TMap<FIndex, AVoxelChunk*>();

	VoxelClasses = TArray<TSubclassOf<UVoxel>>();
	VoxelClasses.Add(UVoxel::StaticClass());
	VoxelClasses.Add(UVoxelEmpty::StaticClass());
	VoxelClasses.Add(UVoxelUnknown::StaticClass());
	VoxelClasses.Add(UVoxelDoor::StaticClass());
	VoxelClasses.Add(UVoxelPlant::StaticClass());
	VoxelClasses.Add(UVoxelTorch::StaticClass());
	VoxelClasses.Add(UVoxelTree::StaticClass());
	VoxelClasses.Add(UVoxelWater::StaticClass());
	
	VoxelGenerators = TMap<TSubclassOf<UVoxelGenerator>, UVoxelGenerator*>();

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TransMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_Trans.M_Voxel_Trans'"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SolidUnlitMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_Solid_Unlit.M_Voxel_Solid_Unlit'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SemiUnlitMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_Semi_Unlit.M_Voxel_Semi_Unlit'"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TransUnlitMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_Trans_Unlit.M_Voxel_Trans_Unlit'"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SolidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_Solid.M_Voxel_Solid'"));
	WorldBasicData.RenderDatas.Add(EVoxelNature::Solid, FVoxelRenderData(SolidMatFinder.Object, SolidUnlitMatFinder.Object, TransMatFinder.Object));
	
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SemiSolidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_SemiSolid.M_Voxel_SemiSolid'"));
	WorldBasicData.RenderDatas.Add(EVoxelNature::SemiSolid, FVoxelRenderData(SemiSolidMatFinder.Object, SemiUnlitMatFinder.Object, TransMatFinder.Object));
	WorldBasicData.RenderDatas.Add(EVoxelNature::SmallSemiSolid, FVoxelRenderData(SemiSolidMatFinder.Object, SemiUnlitMatFinder.Object, TransMatFinder.Object));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TransSolidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_TransSolid.M_Voxel_TransSolid'"));
	WorldBasicData.RenderDatas.Add(EVoxelNature::TransSolid, FVoxelRenderData(TransSolidMatFinder.Object, TransUnlitMatFinder.Object, TransMatFinder.Object));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LiquidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_Liquid.M_Voxel_Liquid'"));
	WorldBasicData.RenderDatas.Add(EVoxelNature::Liquid, FVoxelRenderData(LiquidMatFinder.Object, TransUnlitMatFinder.Object, TransMatFinder.Object));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SemiLiquidMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_SemiLiquid.M_Voxel_SemiLiquid'"));
	WorldBasicData.RenderDatas.Add(EVoxelNature::SemiLiquid, FVoxelRenderData(SemiLiquidMatFinder.Object, TransUnlitMatFinder.Object, TransMatFinder.Object));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FoliageMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_Foliage.M_Voxel_Foliage'"));
	WorldBasicData.RenderDatas.Add(EVoxelNature::Foliage, FVoxelRenderData(FoliageMatFinder.Object, SemiUnlitMatFinder.Object, TransMatFinder.Object));
	
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SemiFoliageMatFinder(TEXT("Material'/WHFramework/Voxel/Materials/M_Voxel_SemiFoliage.M_Voxel_SemiFoliage'"));
	WorldBasicData.RenderDatas.Add(EVoxelNature::SemiFoliage, FVoxelRenderData(SemiFoliageMatFinder.Object, TransUnlitMatFinder.Object, TransMatFinder.Object));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IconSourceMatFinder(TEXT("/Script/Engine.Material'/WHFramework/Voxel/Materials/M_VoxelIcon.M_VoxelIcon'"));
	WorldBasicData.IconMat = IconSourceMatFinder.Object;
}

UVoxelModule::~UVoxelModule()
{
	TERMINATION_MODULE(UVoxelModule)
}

#if WITH_EDITOR
void UVoxelModule::OnGenerate()
{
	if(WorldBasicData.VoxelGenerators.IsEmpty())
	{
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelTemperatureGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelHumidityGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelBiomeGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelHeightGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelCaveGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelOreGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelTerrainGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelRainGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelFoliageGenerator>(this));
		WorldBasicData.VoxelGenerators.Add(NewObject<UVoxelBuildingGenerator>(this));
	}

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
				if(Iter3.Texture && WorldBasicData.RenderDatas.Contains(Iter1->Nature))
				{
					Iter3.UVOffset = FVector2D(0.f, WorldBasicData.RenderDatas[Iter1->Nature].Textures.AddUnique(Iter3.Texture));
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
			
			MatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, Iter.Value.TransMaterial);
			MatInst->SetTextureParameterValue(FName("Texture"), Texture);
			Iter.Value.TransMaterialInst = MatInst;
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
	return FString::Printf(TEXT("WorldState: %s%s"), *UCommonStatics::GetEnumAuthoredNameByValue(TEXT("/Script/WHFramework.EVoxelWorldState"), (int32)WorldState), GetChunkQueues(WorldState).Queues.Num() > 1 ? *FString::Printf(TEXT("_%d"), GetChunkQueues(WorldState).Stage) : TEXT(""));
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
	return WorldData ? *WorldData : *new FVoxelWorldSaveData();
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
		
		for(auto& Iter : WorldData->VoxelGenerators)
		{
			Iter->Initialize(this);
			VoxelGenerators.Add(Iter->GetClass(), Iter);
		}

		VoxelCapture->GetCapture()->SetActive(true);
		VoxelCapture->GetCapture()->OrthoWidth = WorldData->BlockSize * 4.f;
		
		int32 ItemIndex = 0;
		ITER_ARRAY(UAssetModuleStatics::LoadPrimaryAssets<UVoxelData>(FName("Voxel")), Item,
			if(Item->IsUnknown() || !Item->IsMainPart()) continue;
			
			AVoxelEntityCapture* VoxelEntity;
			if(CaptureVoxels.IsValidIndex(ItemIndex))
			{
				VoxelEntity = CaptureVoxels[ItemIndex];
			}
			else
			{
				VoxelEntity = UObjectPoolModuleStatics::SpawnObject<AVoxelEntityCapture>();
				VoxelCapture->GetCapture()->ShowOnlyActors.Add(VoxelEntity);
				CaptureVoxels.EmplaceAt(ItemIndex, VoxelEntity);
			}
			if(VoxelEntity)
			{
				FVoxelItem VoxelItem = Item->GetPrimaryAssetId();
				VoxelEntity->LoadSaveData(&VoxelItem);
				VoxelEntity->SetActorLocation(FVector((ItemIndex / 8 - 3.5f) * WorldBasicData.BlockSize * 0.5f, (ItemIndex % 8 - 3.5f) * WorldBasicData.BlockSize * 0.5f, -800.f));
				VoxelEntity->SetActorRotation(FRotator(-70.f, 0.f, -180.f));
				VoxelEntity->GetMeshComponent()->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
				VoxelEntity->GetMeshComponent()->SetRelativeScale3D(FVector(0.3f));

				if(!Item->Icon)
				{
					UMaterialInstanceDynamic* IconMat = UMaterialInstanceDynamic::Create(WorldBasicData.IconMat, nullptr);
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
		USceneModule::Get().SetSeaLevel(SaveData.SeaLevel * SaveData.BlockSize);
	}
}

FSaveData* UVoxelModule::ToData()
{
	static FVoxelWorldSaveData* SaveData;
	SaveData = NewWorldData(WorldData);
	
	ITER_MAP(ChunkMap, Iter,
		if(Iter.Value->IsGenerated())
		{
			SaveData->SetChunkData(Iter.Key, Iter.Value->GetSaveData<FVoxelChunkSaveData>(true));
		}
	)

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

		ITER_MAP(ChunkMap, Iter,
			UObjectPoolModuleStatics::DespawnObject(Iter.Value);
		)
		ChunkMap.Empty();

		VoxelGenerators.Empty();

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
	if(UpdateChunkQueue(EVoxelWorldState::Destroying, [this](FIndex Index){ DestroyChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Destroying);
	}
	// Spawn chunk
	else if(UpdateChunkQueue(EVoxelWorldState::Spawning, [this](FIndex Index){ SpawnChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Spawning);
	}
	// Load chunk map
	else if(UpdateChunkQueue(EVoxelWorldState::MapLoading, [this](FIndex Index){ LoadChunkMap(Index); }))
	{
		SetWorldState(EVoxelWorldState::MapLoading);
	}
	// Build chunk map
	else if(UpdateChunkQueue(EVoxelWorldState::MapBuilding, [this](FIndex Index, int32 Stage){ BuildChunkMap(Index, Stage); }))
	{
		SetWorldState(EVoxelWorldState::MapBuilding);
	}
	// Spawn chunk mesh
	else if(UpdateChunkQueue(EVoxelWorldState::MeshSpawning, [this](FIndex Index, int32 Stage){ SpawnChunkMesh(Index, Stage); }))
	{
		SetWorldState(EVoxelWorldState::MeshSpawning);
	}
	// Build chunk mesh
	else if(UpdateChunkQueue(EVoxelWorldState::MeshBuilding, [this](FIndex Index){ BuildChunkMesh(Index); }))
	{
		SetWorldState(EVoxelWorldState::MeshBuilding);
	}
	// Generate chunk
	else if(UpdateChunkQueue(EVoxelWorldState::Generating, [this](FIndex Index){ GenerateChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Generating);
	}
	// No work
	else
	{
		SetWorldState(EVoxelWorldState::None);
	}
}

AVoxelChunk* UVoxelModule::SpawnChunk(FIndex InIndex, bool bAddToQueue)
{
	AVoxelChunk* Chunk = GetChunkByIndex(InIndex);
	if(!Chunk)
	{
		Chunk = UObjectPoolModuleStatics::SpawnObject<AVoxelChunk>(nullptr, nullptr, ChunkSpawnClass);
		Chunk->Initialize(this, InIndex, ChunkSpawnBatch + (IsOnTheWorld(InIndex) ? 0 : 1));
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
					AddToChunkQueue(EVoxelWorldState::MapLoading, InIndex);
				}
				else if(InIndex.Z == 0)
				{
					AddToChunkQueue(EVoxelWorldState::MapBuilding, InIndex);
				}
			}
			else if(IsOnTheWorld(InIndex))
			{
				Chunk->bBuilded = true;
			}
		}
		if(!Chunk->IsGenerated() && IsOnTheWorld(InIndex))
		{
			AddToChunkQueue(EVoxelWorldState::MeshSpawning, InIndex);
			AddToChunkQueue(EVoxelWorldState::MeshBuilding, InIndex);
			AddToChunkQueue(EVoxelWorldState::Generating, InIndex);
			for(const auto& Iter : Chunk->GetNeighbors())
			{
				if(Iter.Value && Iter.Value->GetBatch() != Chunk->GetBatch())
				{
					AddToChunkQueue(EVoxelWorldState::MeshSpawning, Iter.Value->GetIndex());
					AddToChunkQueue(EVoxelWorldState::MeshBuilding, Iter.Value->GetIndex());
					AddToChunkQueue(EVoxelWorldState::Generating, Iter.Value->GetIndex());
				}
			}
		}
	}
	return Chunk;
}

void UVoxelModule::GenerateChunk(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->Generate(EPhase::Primary);
	}
}

void UVoxelModule::DestroyChunk(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		if(!Chunk->IsGenerated())
		{
			RemoveFromChunkQueue(EVoxelWorldState::MeshSpawning, InIndex);
			RemoveFromChunkQueue(EVoxelWorldState::MeshBuilding, InIndex);
			RemoveFromChunkQueue(EVoxelWorldState::Generating, InIndex);
			for(const auto& Iter : Chunk->GetNeighbors())
			{
				if(Iter.Value && Iter.Value->GetBatch() != Chunk->GetBatch())
				{
					RemoveFromChunkQueue(EVoxelWorldState::MeshSpawning, Iter.Value->GetIndex());
					RemoveFromChunkQueue(EVoxelWorldState::MeshBuilding, Iter.Value->GetIndex());
					RemoveFromChunkQueue(EVoxelWorldState::Generating, Iter.Value->GetIndex());
				}
			}
		}
		UObjectPoolModuleStatics::DespawnObject(Chunk);
		ChunkMap.Remove(InIndex);
	}
}

void UVoxelModule::LoadChunkMap(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->LoadSaveData(WorldData->GetChunkData(InIndex));
	}
}

void UVoxelModule::BuildChunkMap(FIndex InIndex, int32 InStage)
{
	if(AVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->BuildMap(InStage);
	}
}

void UVoxelModule::SpawnChunkMesh(FIndex InIndex, int32 InStage)
{
	if(AVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->SpawnMeshComponents(InStage);
	}
}

void UVoxelModule::BuildChunkMesh(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->BuildMesh();
	}
}

void UVoxelModule::GenerateChunkQueues(bool bFromAgent, bool bForce)
{
	if(bForce) DestroyChunkQueues();
	FIndex GenerateIndex = FIndex::ZeroIndex;
	FVector GenerateOffset = FVector::ZeroVector;
	const auto VoxelAgent  = Cast<IVoxelAgentInterface>(UCommonStatics::GetPlayerPawn() ? UCommonStatics::GetPlayerPawn() : UCommonStatics::GetPlayerController()->GetViewTarget());
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
					if(UMathStatics::IsPointInEllipse2D(Index.ToVector2D() + FVector2D(0.5f), GenerateIndex.ToVector2D(), FVector2D(SpawnRange.X, SpawnRange.Y)))
					{
						if(DestroyQueue.Contains(Index)) DestroyQueue.Remove(Index);
						AddToChunkQueue(EVoxelWorldState::Spawning, Index);
					}
				}
			}
		}
		ITER_ARRAY(DestroyQueue, Item, AddToChunkQueue(EVoxelWorldState::Destroying, Item););
		ChunkGenerateIndex = GenerateIndex;
		ChunkSpawnBatch++;
	}
}

void UVoxelModule::DestroyChunkQueues()
{
	ITER_MAP(ChunkQueues, Item1,
		ITER_ARRAY(Item1.Value.Queues, Item2,
			Item2.Queue.Empty();
			ITER_ARRAY(Item2.Threads, Item3,
				delete Item3;
			)
			Item2.Threads.Empty();
		)
	)
}

bool UVoxelModule::UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex)> InFunc)
{
	return UpdateChunkQueue(InState, [InFunc](FIndex Index, int32 Stage) { InFunc(Index); });
}

bool UVoxelModule::UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex, int32)> InFunc)
{
	ITER_ARRAY_WITHINDEX(ChunkQueues[InState].Queues, i, Item,
		ChunkQueues[InState].Stage = i + 1;
		if(Item.bAsync)
		{
			if(Item.Threads.Num() == 0 && Item.Queue.Num() > 0)
			{
				DON_WITHINDEX(FMath::Min(FMath::CeilToInt((float)Item.Queue.Num() / Item.Speed), FPlatformMisc::NumberOfWorkerThreadsToSpawn()), j,
					TArray<FIndex> Queue;
					DON_WITHINDEX(FMath::Min(Item.Speed, Item.Queue.Num() - j * Item.Speed), k,
						Queue.Add(Item.Queue[j * Item.Speed + k]);
					)
					const auto Thread = new FVoxelChunkQueueThread(Queue, InFunc, i + 1);
					Item.Threads.Add(Thread);
				)
			}
			else if(Item.Threads.Num() > 0)
			{
				const auto Threads = Item.Threads;
				ITER_ARRAY(Threads, Thread,
					if(Thread->IsFinished())
					{
						ITER_ARRAY(Thread->GetChunkQueue(), Index,
							Item.Queue.Remove(Index);
						)
						Item.Threads.Remove(Thread);
						delete Thread;
					}
				)
			}
			if(Item.Queue.Num() > 0 || Item.Threads.Num() > 0)
			{
				return true;
			}
		}
		else
		{
			const int32 Num = FMath::Min(Item.Speed, Item.Queue.Num());
			DON_WITHINDEX(Num, j, InFunc(Item.Queue[j], i + 1); )
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
		if(!Item.Queue.Contains(InIndex) && (InState == EVoxelWorldState::Spawning ? !ChunkMap.Contains(InIndex) : ChunkMap.Contains(InIndex)))
		{
			Item.Queue.Add(InIndex);
			switch(InState)
			{
				case EVoxelWorldState::Generating:
				{
					Item.Queue.Sort([this](const FIndex& A, const FIndex& B){
						const float LengthA = ChunkGenerateIndex.DistanceTo(A, false, true);
						const float LengthB = ChunkGenerateIndex.DistanceTo(B, false, true);
						return LengthA < LengthB;
					});
					break;
				}
				case EVoxelWorldState::Destroying:
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

bool UVoxelModule::GenerateVoxel(AVoxelChunk* InChunk, const TSubclassOf<UVoxelGenerator>& InClass) const
{
	if(UVoxelGenerator* VoxelGenerator = GetVoxelGenerator(InClass))
	{
		VoxelGenerator->Generate(InChunk);
		return true;
	}
	return false;
}

bool UVoxelModule::IsOnTheWorld(FIndex InIndex, bool bIgnoreZ) const
{
	const FVector SpawnRange = FVector(WorldData->GetWorldSize().X * 0.5f, WorldData->GetWorldSize().Y * 0.5f, WorldData->GetWorldSize().Z);
	return InIndex.X >= ChunkGenerateIndex.X - SpawnRange.X && InIndex.X < ChunkGenerateIndex.X + SpawnRange.X &&
		InIndex.Y >= ChunkGenerateIndex.Y - SpawnRange.Y && InIndex.Y < ChunkGenerateIndex.Y + SpawnRange.Y &&
		(!bIgnoreZ || InIndex.Z >= ChunkGenerateIndex.Z && InIndex.Z < ChunkGenerateIndex.Z + SpawnRange.Z);
}

AVoxelChunk* UVoxelModule::GetChunkByIndex(FIndex InIndex) const
{
	if(ChunkMap.Contains(InIndex))
	{
		return ChunkMap[InIndex];
	}
	return nullptr;
}

AVoxelChunk* UVoxelModule::GetChunkByLocation(FVector InLocation) const
{
	return GetChunkByIndex(LocationToChunkIndex(InLocation));
}

AVoxelChunk* UVoxelModule::GetChunkByVoxelIndex(FIndex InIndex) const
{
	return GetChunkByIndex(VoxelIndexToChunkIndex(InIndex));
}

bool UVoxelModule::HasVoxelByIndex(FIndex InIndex, bool bSafe)
{
	if(AVoxelChunk* Chunk = GetChunkByVoxelIndex(InIndex))
	{
		return Chunk->HasVoxel(Chunk->WorldIndexToLocal(InIndex), bSafe);
	}
	return false;
}

bool UVoxelModule::HasVoxelByLocation(FVector InLocation, bool bSafe)
{
	return HasVoxelByIndex(LocationToVoxelIndex(InLocation), bSafe);
}

FVoxelItem& UVoxelModule::GetVoxelByIndex(FIndex InIndex, bool bMainPart)
{
	if(AVoxelChunk* Chunk = GetChunkByVoxelIndex(InIndex))
	{
		return Chunk->GetVoxel(Chunk->WorldIndexToLocal(InIndex), bMainPart);
	}
	return FVoxelItem::Empty;
}

FVoxelItem& UVoxelModule::GetVoxelByLocation(FVector InLocation, bool bMainPart)
{
	return GetVoxelByIndex(LocationToVoxelIndex(InLocation), bMainPart);
}

void UVoxelModule::SetVoxelByIndex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bSafe)
{
	if(AVoxelChunk* Chunk = GetChunkByVoxelIndex(InIndex))
	{
		Chunk->SetVoxel(Chunk->WorldIndexToLocal(InIndex), InVoxelItem, bSafe);
	}
}

void UVoxelModule::SetVoxelByLocation(FVector InLocation, const FVoxelItem& InVoxelItem, bool bSafe)
{
	SetVoxelByIndex(LocationToVoxelIndex(InLocation), InVoxelItem, bSafe);
}

FVoxelTopography& UVoxelModule::GetTopographyByIndex(FIndex InIndex)
{
	if(AVoxelChunk* Chunk = GetChunkByVoxelIndex(FIndex(InIndex.X, InIndex.Y, 0)))
	{
		return Chunk->GetTopography(Chunk->WorldIndexToLocal(FIndex(InIndex.X, InIndex.Y, 0)));
	}
	return *new FVoxelTopography();
}

FVoxelTopography& UVoxelModule::GetTopographyByLocation(FVector InLocation)
{
	return GetTopographyByIndex(LocationToVoxelIndex(InLocation));
}

void UVoxelModule::SetTopographyByIndex(FIndex InIndex, const FVoxelTopography& InTopography)
{
	if(AVoxelChunk* Chunk = GetChunkByVoxelIndex(FIndex(InIndex.X, InIndex.Y, 0)))
	{
		Chunk->SetTopography(Chunk->WorldIndexToLocal(FIndex(InIndex.X, InIndex.Y, 0)), InTopography);
	}
}

void UVoxelModule::SetTopographyLocation(FVector InLocation, const FVoxelTopography& InTopography)
{
	SetTopographyLocation(LocationToVoxelIndex(InLocation), InTopography);
}

float UVoxelModule::GetVoxelNoise1D(float InValue, bool bAbs, bool bUnsigned) const
{
	return UMathStatics::GetNoise1D(InValue, WorldData->WorldSeed, bAbs, bUnsigned);
}

float UVoxelModule::GetVoxelNoise2D(FVector2D InLocation, bool bAbs, bool bUnsigned) const
{
	return UMathStatics::GetNoise2D(InLocation, WorldData->WorldSeed, bAbs, bUnsigned);
}

float UVoxelModule::GetVoxelNoise3D(FVector InLocation, bool bAbs, bool bUnsigned) const
{
	return UMathStatics::GetNoise3D(InLocation, WorldData->WorldSeed, bAbs, bUnsigned);
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

FIndex UVoxelModule::ChunkIndexToVoxelIndex(FIndex InIndex) const
{
	return InIndex * WorldData->ChunkSize;
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

FIndex UVoxelModule::VoxelIndexToChunkIndex(FIndex InIndex) const
{
	const FVector Index = InIndex.ToVector() / WorldData->ChunkSize;
	return FIndex(FMath::FloorToInt(Index.X), FMath::FloorToInt(Index.Y), FMath::FloorToInt(Index.Z));
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

bool UVoxelModule::VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter, bool bForce)
{
	const FVector ChunkRadius = WorldData->GetChunkRealSize() * 0.5f;
	const FVector ChunkLocation = ChunkIndexToLocation(InChunkIndex);
	return VoxelAgentTraceSingle(ChunkLocation + ChunkRadius, FVector2D(ChunkRadius.X, ChunkRadius.Y), InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter, bForce);
}

bool UVoxelModule::VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter, bool bForce)
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
		if(VoxelAgentTraceSingle(RayStart, RayEnd, InRadius, InHalfHeight, InIgnoreActors, HitResult, !bForce || i < InMaxCount - 1))
		{
			OutHitResult = HitResult;
			return true;
		}
	)
	return false;
}

bool UVoxelModule::VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bCheckVoxel)
{
	FHitResult HitResult1;
	if(UKismetSystemLibrary::CapsuleTraceSingle(GetWorldContext(), InRayStart, InRayEnd, InRadius * 0.95f, InHalfHeight, USceneModuleStatics::GetTraceMapping(FName("Chunk")).GetTraceType(), false, InIgnoreActors, EDrawDebugTrace::None, HitResult1, true))
	{
		FHitResult HitResult2;
		if(!UKismetSystemLibrary::CapsuleTraceSingle(GetWorldContext(), HitResult1.Location, HitResult1.Location, InRadius * 0.95f, InHalfHeight * 0.95f, USceneModuleStatics::GetTraceMapping(FName("Voxel")).GetTraceType(), false, InIgnoreActors, EDrawDebugTrace::None, HitResult2, true))
		{
			if(!bCheckVoxel || !GetVoxelByLocation(HitResult1.Location).IsValid())
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
	const FVector Range = FVector(FMath::Min(ChunkSpawnDistance, WorldData->GetWorldSize().X * 0.5f), FMath::Min(ChunkSpawnDistance, WorldData->GetWorldSize().Y * 0.5f), WorldData->GetWorldSize().Z);
	ITER_INDEX(Iter, Range, true,
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
		ITER_MAP(ChunkMap, Iter,
			if(Iter.Value->IsGenerated())
			{
				ReturnValue++;
			}
		)
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
	return GetChunkByIndex(InIndex) && GetChunkByIndex(InIndex)->IsGenerated();
}

TArray<AVoxelChunk*> UVoxelModule::GetVerticalChunks(FIndex InIndex) const
{
	TArray<AVoxelChunk*> ReturnValue;
	for(int32 i = 0; i < WorldData->WorldSize.Z; i++)
	{
		const FIndex Index = FIndex(InIndex.X, InIndex.Y, i);
		if(AVoxelChunk* Chunk = GetChunkByIndex(Index))
		{
			ReturnValue.Add(Chunk);
		}
	}
	return ReturnValue;
}

FVoxelChunkQueues UVoxelModule::GetChunkQueues(EVoxelWorldState InWorldState) const
{
	if(ChunkQueues.Contains(InWorldState))
	{
		return ChunkQueues[InWorldState];
	}
	return FVoxelChunkQueues();
}

UVoxelGenerator* UVoxelModule::GetVoxelGenerator(const TSubclassOf<UVoxelGenerator>& InClass) const
{
	if(VoxelGenerators.Contains(InClass))
	{
		return VoxelGenerators[InClass];
	}
	return nullptr;
}
