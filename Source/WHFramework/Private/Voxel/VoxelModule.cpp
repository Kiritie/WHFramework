#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"

#include "Ability/AbilityModuleStatics.h"
#include "Asset/AssetModuleStatics.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformMisc.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Voxel/Event_VoxelWorldModeChanged.h"
#include "Event/Events/Voxel/Event_VoxelWorldStateChanged.h"
#include "Engine/Texture2D.h"
#include "Main/MainModuleStatics.h"
#include "Math/MathHelper.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "ReferencePool/ReferencePoolModuleStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelDoor.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/VoxelTorch.h"
#include "Voxel/Voxels/VoxelWater.h"
#include "Voxel/Voxels/Entity/VoxelEntityCapture.h"
#include "Common/CommonModuleStatics.h"
#include "Common/CommonModuleTypes.h"
#include "Event/Events/Voxel/Event_VoxelWorldAgentMoved.h"
#include "Event/Events/Voxel/Event_VoxelWorldCenterChanged.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Main/MainModule.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/MathTypes.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/VoxelSaveGame.h"
#include "Scene/SceneModule.h"
#include "Voxel/Capture/VoxelCapture.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Generators/VoxelBuildingGenerator.h"
#include "Voxel/Generators/VoxelTownGenerator.h"
#include "Voxel/Generators/VoxelCaveGenerator.h"
#include "Voxel/Generators/VoxelGenerator.h"
#include "Voxel/Generators/VoxelFoliageGenerator.h"
#include "Voxel/Generators/VoxelLakeGenerator.h"
#include "Voxel/Generators/VoxelLiquidGenerator.h"
#include "Voxel/Generators/VoxelOreGenerator.h"
#include "Voxel/Generators/VoxelRegionGenerator.h"
#include "Voxel/Generators/VoxelRiverGenerator.h"
#include "Voxel/Generators/VoxelSurfaceGenerator.h"
#include "Voxel/Generators/VoxelTerrainGenerator.h"
#include "Voxel/Root/VoxelRoot.h"
#include "Voxel/Voxels/VoxelContainer.h"

IMPLEMENTATION_MODULE(UVoxelModule)

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
	WorldCenterIndex = EMPTY_Index;
	WorldAgentIndex = EMPTY_Index;

	WorldData = nullptr;
	VoxelAreaNamespace = NAME_None;

	ChunkSpawnClass = UVoxelChunk::StaticClass();
	
	ChunkSpawnDistance = 0.35f;
	ChunkQueues = {
		{ EVoxelWorldState::Spawning, FVoxelChunkQueues({
			FVoxelChunkQueue(false, 1000)
		}) },
		{ EVoxelWorldState::MapLoading, FVoxelChunkQueues({
			FVoxelChunkQueue(true, 512)
		}) },
		{ EVoxelWorldState::MapBuilding, FVoxelChunkQueues({
			FVoxelChunkQueue(true, 256, {
				CreateDefaultSubobject<UVoxelSurfaceGenerator>(FName("SurfaceGenerator")),
				CreateDefaultSubobject<UVoxelRiverGenerator>(FName("RiverGenerator")),
				CreateDefaultSubobject<UVoxelLakeGenerator>(FName("LakeGenerator")),
				CreateDefaultSubobject<UVoxelRegionGenerator>(FName("RegionGenerator")),
				CreateDefaultSubobject<UVoxelCaveGenerator>(FName("CaveGenerator")),
				CreateDefaultSubobject<UVoxelOreGenerator>(FName("OreGenerator")),
				CreateDefaultSubobject<UVoxelTerrainGenerator>(FName("TerrainGenerator")),
				CreateDefaultSubobject<UVoxelFoliageGenerator>(FName("FoliageGenerator")),
				CreateDefaultSubobject<UVoxelTownGenerator>(FName("TownGenerator")),
				CreateDefaultSubobject<UVoxelBuildingGenerator>(FName("BuildingGenerator"))
			}),
			FVoxelChunkQueue(true, 256, {
				CreateDefaultSubobject<UVoxelLiquidGenerator>(FName("LiquidGenerator"))
			})
		}) },
		{ EVoxelWorldState::MeshSpawning, FVoxelChunkQueues({
			FVoxelChunkQueue(true, 512),
			FVoxelChunkQueue(false, 30)
		}) },
		{ EVoxelWorldState::MeshBuilding, FVoxelChunkQueues({
			FVoxelChunkQueue(false, 4)
		}) },
		{ EVoxelWorldState::Generating, FVoxelChunkQueues({
			FVoxelChunkQueue(false, 1)
		}) },
		{ EVoxelWorldState::Unloading, FVoxelChunkQueues({
			FVoxelChunkQueue(false, 10)
		}) }
	};

	ChunkQueueThreads = TArray<FVoxelChunkQueueThread*>();
	ActiveChunkQueueBatch.Reset();
	ActiveChunkQueue = nullptr;
	ActiveChunkQueueThreads = TArray<FVoxelChunkQueueThread*>();
	ActiveChunkQueueGenerators = TArray<UVoxelGenerator*>();

	ChunkSpawnBatch = 0;
	ChunkMap = TMap<FIndex, UVoxelChunk*>();
	VoxelUpdateChunkIndices = TSet<FIndex>();
	VoxelLiquidUpdateIndices = TSet<FIndex>();
	VoxelUpdateTime = 0.f;
	bVoxelUpdateRunning = false;

	VoxelClasses = TArray<TSubclassOf<UVoxel>>();
	VoxelClasses.Add(UVoxel::StaticClass());
	VoxelClasses.Add(UVoxelEmpty::StaticClass());
	VoxelClasses.Add(UVoxelUnknown::StaticClass());
	VoxelClasses.Add(UVoxelInteract::StaticClass());
	VoxelClasses.Add(UVoxelSwitch::StaticClass());
	VoxelClasses.Add(UVoxelContainer::StaticClass());
	VoxelClasses.Add(UVoxelDoor::StaticClass());
	VoxelClasses.Add(UVoxelPlant::StaticClass());
	VoxelClasses.Add(UVoxelTorch::StaticClass());
	VoxelClasses.Add(UVoxelWater::StaticClass());
	
	VoxelGeneratorMap = TMap<TSubclassOf<UVoxelGenerator>, UVoxelGenerator*>();
	VoxelAssetIDMap = TMap<EVoxelType, FPrimaryAssetId>();

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
	if(!VoxelRoot)
	{
		TArray<AActor*> ChildActors;
		GetModuleOwner()->GetAttachedActors(ChildActors);
		if(ChildActors.Num() > 0)
		{
			VoxelRoot = Cast<AVoxelRoot>(ChildActors[0]);
		}
	}
	if(!VoxelRoot)
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		VoxelRoot = GetWorld()->SpawnActor<AVoxelRoot>(ActorSpawnParameters);
		if(VoxelRoot)
		{
			VoxelRoot->SetActorLabel(TEXT("VoxelRoot"));
			VoxelRoot->AttachToActor(GetModuleOwner(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	if(VoxelRoot)
	{
		VoxelRoot->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
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

	ShutdownChunkQueueThreads();
	TERMINATION_MODULE(UVoxelModule)

	if(VoxelRoot)
	{
		VoxelRoot->Destroy();
	}

	if(VoxelCapture)
	{
		VoxelCapture->Destroy();
	}
}
#endif

void UVoxelModule::OnInitialize()
{
	Super::OnInitialize();


	
	USceneModule::Get().RegisterSceneAreaResolver(ESceneAreaType::Chunk, FSceneAreaResolver::CreateUObject(this, &UVoxelModule::ResolveVoxelArea));

	UAssetModuleStatics::AddStaticObject(FName("EVoxelType"), FStaticObject(UEnum::StaticClass(), TEXT("/Script/WHFramework.EVoxelType")));

	USceneModuleStatics::AddTraceMapping(FName("Chunk"), (ECollisionChannel)EGameTraceChannel::Chunk);
	USceneModuleStatics::AddTraceMapping(FName("Voxel"), (ECollisionChannel)EGameTraceChannel::Voxel);

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

		if(UTexture2D* Texture = UCommonModuleStatics::CompositeTextures(Iter.Value.Textures, Iter.Value.TextureSize))
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
		
	for(auto& Iter : ChunkQueues)
	{
		ITER_ARRAY_WITHINDEX(Iter.Value.Queues, i, Queue,
			for(UVoxelGenerator* Generator : Queue.Generators)
			{
				Generator->Initialize(this, i + 1);
				VoxelGeneratorMap.Add(Generator->GetClass(), Generator);
			}
		)
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
		FIndex VoxelUpdateIndex;
		while(VoxelUpdateQueue.Dequeue(VoxelUpdateIndex)) AddToVoxelUpdateQueue(VoxelUpdateIndex);
		VoxelUpdateTime += DeltaSeconds;
		if(VoxelUpdateTime >= 0.2f && !bVoxelUpdateRunning)
		{
			VoxelUpdateTime = 0.f;
			UpdateVoxelQueue();
		}
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
		ShutdownChunkQueueThreads();
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
	const FString StateName = UCommonModuleStatics::GetEnumAuthoredNameByValue(TEXT("/Script/WHFramework.EVoxelWorldState"), static_cast<int32>(WorldState));
	return FString::Printf(TEXT("WorldState: %s"), *StateName);
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
	UEventModuleStatics::BroadcastEvent<FEventVoxelWorldModeChanged>(this, { WorldMode });
}

void UVoxelModule::OnWorldStateChanged()
{
	UEventModuleStatics::BroadcastEvent<FEventVoxelWorldStateChanged>(this, { WorldState });
}

void UVoxelModule::OnWorldCenterChanged()
{
	UEventModuleStatics::BroadcastEvent<FEventVoxelWorldCenterChanged>(this, { WorldCenterIndex });
}

void UVoxelModule::OnWorldAgentMoved()
{
	UEventModuleStatics::BroadcastEvent<FEventVoxelWorldAgentMoved>(this, { WorldAgentIndex });
}

float UVoxelModule::GetWorldGeneratePercent() const
{
	const int32 BasicNum = (WorldData->GetWorldSize().X * ChunkSpawnDistance) * (WorldData->GetWorldSize().Y * ChunkSpawnDistance);
	int32 GeneratedNum = 0;
	ITER_MAP(ChunkMap, Iter,
		if(Iter.Value->IsGenerated())
		{
			GeneratedNum++;
		}
	)
	return (float)GeneratedNum / BasicNum;
}

FBox UVoxelModule::GetWorldBounds(float InRadius, float InHalfHeight) const
{
	const FVector2D WorldRadius = WorldData->GetWorldRealSize() * 0.5f;
	const FVector WorldCenter = FVector(ChunkIndexToLocation(WorldCenterIndex).X + ((int32)WorldData->GetWorldSize().X % 2 == 1 ? WorldData->GetChunkRealSize().X * 0.5f : 0.f), ChunkIndexToLocation(WorldCenterIndex).Y + ((int32)WorldData->GetWorldSize().Y % 2 == 1 ? WorldData->GetChunkRealSize().Y * 0.5f : 0.f), WorldData->SkyHeight);
	return FBox(WorldCenter - FVector(WorldRadius.X - InRadius, WorldRadius.Y - InRadius, WorldData->GetWorldRealHeight() - InHalfHeight), WorldCenter + FVector(WorldRadius.X - InRadius, WorldRadius.Y - InRadius, WorldData->GetWorldRealHeight() - InHalfHeight));
}

FVoxelWorldSaveData& UVoxelModule::GetWorldData() const
{
	return WorldData ? *WorldData : GetMutableSaveData<FVoxelWorldSaveData>();
}

FVoxelWorldSaveData* UVoxelModule::NewWorldData(FSaveData* InBasicData) const
{
	FVoxelModuleSaveData& SaveData = GetMutableSaveData<FVoxelModuleSaveData>();
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
			if(!VoxelAssetIDMap.Contains(Item->VoxelType))
			{
				VoxelAssetIDMap.Add(Item->VoxelType, Item->GetPrimaryAssetId());
			}
		
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

				if(UMaterialInstanceDynamic* IconMat = Cast<UMaterialInstanceDynamic>(Item->Icon))
				{
					IconMat->SetTextureParameterValue(FName("Texture"), VoxelCapture->GetCapture()->TextureTarget);
					IconMat->SetScalarParameterValue(FName("Index"), ItemIndex);
					IconMat->SetScalarParameterValue(FName("SizeX"), 8.f);
					IconMat->SetScalarParameterValue(FName("SizeY"), 8.f);
				}
			}
			ItemIndex++;
		)
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
	FVoxelWorldSaveData* SaveData = NewWorldData(WorldData);
	
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
		SetWorldState(EVoxelWorldState::None);

		ResetChunkQueues();

		ITER_MAP(ChunkMap, Iter,
			UObjectPoolModuleStatics::DespawnObject(Iter.Value);
		)
		ChunkMap.Empty();

		ChunkSpawnBatch = 0;
		WorldCenterIndex = EMPTY_Index;
		WorldAgentIndex = EMPTY_Index;

		WorldData = NewWorldData();

		VoxelCapture->GetCapture()->SetActive(false);
		
		VoxelAssetIDMap.Empty();
	}
}

void UVoxelModule::LoadPrefabData(const FVoxelPrefabSaveData& InPrefabData)
{
	if(WorldMode != EVoxelWorldMode::Prefab || GetWorldGeneratePercent() < 1.f) return;
	
	TArray<UVoxelChunk*> GenerateChunks;
	ITER_MAP(ChunkMap, Iter,
		if(Iter.Value->IsGenerated() && Iter.Value->IsChanged())
		{
			Iter.Value->ClearMap();
			GenerateChunks.Add(Iter.Value);
		}
	)
	if(!InPrefabData.VoxelDatas.IsEmpty())
	{
		TArray<FString> VoxelDatas;
		InPrefabData.VoxelDatas.ParseIntoArray(VoxelDatas, TEXT("|"));
		for(auto& Iter : VoxelDatas)
		{
			FVoxelItem VoxelItem(Iter, true);
			if(VoxelItem.IsValid())
			{
				if(UVoxelChunk* Chunk = GetChunkByVoxelIndex(VoxelItem.Index))
				{
					SetVoxelByIndex(VoxelItem.Index, VoxelItem);
					GenerateChunks.AddUnique(Chunk);
				}
			}
		}
	}
	for(auto Iter : GenerateChunks)
	{
		if(Iter)
		{
			Iter->Generate(EPhase::Lesser);
			Iter->SetChanged(true);
		}
	}
}

FVoxelPrefabSaveData UVoxelModule::GetPrefabData()
{
	if(WorldMode != EVoxelWorldMode::Prefab || GetWorldGeneratePercent() < 1.f) return FVoxelPrefabSaveData();
	
	FVoxelPrefabSaveData PrefabData;
	ITER_MAP(ChunkMap, Iter,
		if(Iter.Value->IsGenerated() && Iter.Value->IsChanged())
		{
			for(auto& VoxelIter : Iter.Value->VoxelMap)
			{
				FVoxelItem& Item = VoxelIter.Value;
				if(Item.IsValid())
				{
					PrefabData.VoxelDatas.Appendf(TEXT("%s|"), *Item.ToSaveData(true, true));
				}
			}
		}
	)
	PrefabData.VoxelDatas.RemoveFromEnd(TEXT("|"));
	return PrefabData;
}

void UVoxelModule::GenerateWorld()
{
	UpdateChunkQueueThreads();
	if(ActiveChunkQueueBatch) return;

	if(UpdateChunkQueue(EVoxelWorldState::Unloading, [this](FIndex Index){ UnloadChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Unloading);
	}
	else if(UpdateChunkQueue(EVoxelWorldState::Spawning, [this](FIndex Index){ SpawnChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Spawning);
	}
	else if(UpdateChunkQueue(EVoxelWorldState::MapLoading, [this](FIndex Index){ LoadChunkMap(Index); }))
	{
		SetWorldState(EVoxelWorldState::MapLoading);
	}
	else if(UpdateChunkQueue(EVoxelWorldState::MapBuilding, [this](FIndex Index, int32 Stage){ BuildChunkMap(Index, Stage); }))
	{
		SetWorldState(EVoxelWorldState::MapBuilding);
	}
	else if(UpdateChunkQueue(EVoxelWorldState::MeshSpawning, [this](FIndex Index, int32 Stage){ SpawnChunkMesh(Index, Stage); }))
	{
		SetWorldState(EVoxelWorldState::MeshSpawning);
	}
	else if(UpdateChunkQueue(EVoxelWorldState::MeshBuilding, [this](FIndex Index){ BuildChunkMesh(Index); }))
	{
		SetWorldState(EVoxelWorldState::MeshBuilding);
	}
	else if(UpdateChunkQueue(EVoxelWorldState::Generating, [this](FIndex Index){ GenerateChunk(Index); }))
	{
		SetWorldState(EVoxelWorldState::Generating);
	}
	else
	{
		SetWorldState(EVoxelWorldState::None);
	}
}

UVoxelChunk* UVoxelModule::SpawnChunk(FIndex InIndex, bool bAddToQueue)
{
	UVoxelChunk* Chunk = GetChunkByIndex(InIndex);
	if(!Chunk)
	{
		Chunk = UObjectPoolModuleStatics::SpawnObject<UVoxelChunk>(nullptr, nullptr, ChunkSpawnClass);
		Chunk->Initialize(this, InIndex, ChunkSpawnBatch + !IsOnTheWorld(InIndex));
		ChunkMap.Add(InIndex, Chunk);
	}
	if(bAddToQueue)
	{
		if(!Chunk->IsBuilded())
		{
			const FVoxelChunkSaveData* ChunkData = WorldData->GetChunkData(InIndex);
			if(ChunkData && ChunkData->bChanged)
			{
				AddToChunkQueue(EVoxelWorldState::MapLoading, InIndex);
			}
			else
			{
				AddToChunkQueue(EVoxelWorldState::MapBuilding, InIndex);
			}
		}
		if(!Chunk->IsGenerated())
		{
			TArray<UVoxelChunk*> GenerateChunks;
			Chunk->GetNeighbors().GenerateValueArray(GenerateChunks);
			GenerateChunks.Add(Chunk);
			for(auto Iter : GenerateChunks)
			{
				if(Iter && (Iter == Chunk || Iter->GetBatch() != Chunk->GetBatch()))
				{
					AddToChunkQueue(EVoxelWorldState::MeshSpawning, Iter->GetIndex());
					AddToChunkQueue(EVoxelWorldState::MeshBuilding, Iter->GetIndex());
					AddToChunkQueue(EVoxelWorldState::Generating, Iter->GetIndex());
				}
			}
		}
	}
	return Chunk;
}

void UVoxelModule::LoadChunkMap(FIndex InIndex)
{
	if(UVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->LoadSaveData(WorldData->GetChunkData(InIndex));
	}
}

void UVoxelModule::BuildChunkMap(FIndex InIndex, int32 InStage)
{
	if(UVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->BuildMap(InStage);
	}
}

void UVoxelModule::SpawnChunkMesh(FIndex InIndex, int32 InStage)
{
	if(UVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->SpawnMeshComponents(InStage);
	}
}

void UVoxelModule::BuildChunkMesh(FIndex InIndex)
{
	if(UVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->BuildMesh();
	}
}

void UVoxelModule::GenerateChunk(FIndex InIndex)
{
	if(UVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->Generate(EPhase::Primary);
	}
}

void UVoxelModule::SaveChunk(FIndex InIndex)
{
	if(UVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		Chunk->SaveData();
	}
}

void UVoxelModule::UnloadChunk(FIndex InIndex)
{
	if(UVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		if(!Chunk->IsGenerated())
		{
			TArray<UVoxelChunk*> NeighborChunks;
			Chunk->GetNeighbors().GenerateValueArray(NeighborChunks);
			NeighborChunks.Add(Chunk);
			for(auto Iter : NeighborChunks)
			{
				if(Iter && (Iter == Chunk || Iter->GetBatch() != Chunk->GetBatch()))
				{
					RemoveFromChunkQueue(EVoxelWorldState::MeshSpawning, Iter->GetIndex());
					RemoveFromChunkQueue(EVoxelWorldState::MeshBuilding, Iter->GetIndex());
					RemoveFromChunkQueue(EVoxelWorldState::Generating, Iter->GetIndex());
				}
			}
		}
		UObjectPoolModuleStatics::DespawnObject(Chunk);
		ChunkMap.Remove(InIndex);
	}
}

void UVoxelModule::GenerateChunkQueues(bool bFromAgent, bool bForce)
{
	if(bForce) ResetChunkQueues();
	FIndex GenerateIndex = FIndex::ZeroIndex;
	FVector2D GenerateOffset = FVector2D::ZeroVector;
	AActor* VoxelAgentActor = UCommonModuleStatics::GetPlayerPawn();
	if(!VoxelAgentActor)
	{
		if(APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController()) VoxelAgentActor = PlayerController->GetViewTarget();
	}
	const auto VoxelAgent = Cast<IVoxelAgentInterface>(VoxelAgentActor);
	if(bFromAgent && VoxelAgent)
	{
		const FVector2D AgentLocation = FVector2D(WorldData->WorldRange.X != 0.f ? VoxelAgent->GetVoxelAgentLocation().X : 0.f, WorldData->WorldRange.Y != 0.f ? VoxelAgent->GetVoxelAgentLocation().Y : 0.f);
		GenerateIndex = LocationToChunkIndex(FVector(AgentLocation.X, AgentLocation.Y, 0.f));
		GenerateOffset = (AgentLocation / WorldData->GetChunkRealSize() - WorldCenterIndex.ToVector2D()).GetAbs();
		if(WorldAgentIndex != GenerateIndex)
		{
			WorldAgentIndex = GenerateIndex;
			OnWorldAgentMoved();
		}
	}
	if(bForce || WorldCenterIndex == EMPTY_Index || (WorldData->WorldRange.X != 0.f && GenerateOffset.X > WorldData->GetWorldSize().X * ChunkSpawnDistance * 0.5f) || (WorldData->WorldRange.Y != 0.f && GenerateOffset.Y > WorldData->GetWorldSize().Y * ChunkSpawnDistance * 0.5f))
	{
		TSet<FIndex> UnloadIndices;
		for(const auto& Iter : ChunkMap) UnloadIndices.Add(Iter.Key);
		const FVector2D SpawnRange = WorldData->GetWorldSize() * 0.5f;
		for(int32 x = GenerateIndex.X - SpawnRange.X; x < GenerateIndex.X + SpawnRange.X; x++)
		{
			for(int32 y = GenerateIndex.Y - SpawnRange.Y; y < GenerateIndex.Y + SpawnRange.Y; y++)
			{
				const FIndex Index = FIndex(x, y, 0);
				if(FMathHelper::IsPointInEllipse2D(Index.ToVector2D() + FVector2D(0.5f), GenerateIndex.ToVector2D(), FVector2D(FMath::CeilToInt(SpawnRange.X), FMath::CeilToInt(SpawnRange.Y))))
				{
					UnloadIndices.Remove(Index);
					AddToChunkQueue(EVoxelWorldState::Spawning, Index);
				}
			}
		}
		ITER_ARRAY(UnloadIndices, Item,
			AddToChunkQueue(EVoxelWorldState::Unloading, Item);
		)
		WorldCenterIndex = GenerateIndex;
		ITER_ARRAY(ChunkQueues[EVoxelWorldState::Generating].Queues, Queue, Queue.bSortRequired = true; )
		ITER_ARRAY(ChunkQueues[EVoxelWorldState::Unloading].Queues, Queue, Queue.bSortRequired = true; )
		ChunkSpawnBatch++;
		
		OnWorldCenterChanged();
	}
}

void UVoxelModule::ResetChunkQueues()
{
	CancelChunkQueueBatch();

	for(auto& Iter : ChunkQueues)
	{
		ITER_ARRAY(Iter.Value.Queues, Queue,
			Queue.Reset();
		)
		Iter.Value.Stage = 0;
	}
}

void UVoxelModule::UpdateChunkQueueThreads()
{
	if(!ActiveChunkQueueBatch) return;

	for(const FVoxelChunkQueueThread* Thread : ActiveChunkQueueThreads)
	{
		if(Thread && !Thread->IsIdle()) return;
	}

	const bool bCancelled = ActiveChunkQueueBatch->IsCancelled();
	for(UVoxelGenerator* Generator : ActiveChunkQueueGenerators)
	{
		if(Generator) Generator->CompleteBatch(bCancelled);
	}
	if(ActiveChunkQueue && !bCancelled)
	{
		TSet<FIndex> CompletedIndices;
		CompletedIndices.Reserve(ActiveChunkQueueBatch->GetQueue().Num());
		for(const FIndex& Index : ActiveChunkQueueBatch->GetQueue()) CompletedIndices.Add(Index);
		ActiveChunkQueue->RemoveBatch(CompletedIndices);
	}
	ActiveChunkQueueBatch.Reset();
	ActiveChunkQueue = nullptr;
	ActiveChunkQueueThreads.Empty();
	ActiveChunkQueueGenerators.Empty();
}

bool UVoxelModule::DispatchChunkQueue(FVoxelChunkQueue& InQueue, const TFunction<void(FIndex, int32)>& InFunc, int32 InStage, const TArray<UVoxelGenerator*>& InGenerators)
{
	if(ActiveChunkQueueBatch || InQueue.Queue.Num() == 0) return false;

	const int32 BatchCount = FMath::Min(FMath::Max(1, InQueue.Speed), InQueue.Queue.Num());
	const int32 WorkerCount = FMath::Min(FMath::Max(1, FPlatformMisc::NumberOfWorkerThreadsToSpawn()), BatchCount);
	while(ChunkQueueThreads.Num() < WorkerCount)
	{
		FVoxelChunkQueueThread* Thread = new FVoxelChunkQueueThread();
		if(!Thread->IsValid())
		{
			delete Thread;
			break;
		}
		ChunkQueueThreads.Add(Thread);
	}
	if(ChunkQueueThreads.Num() == 0) return false;

	TArray<FIndex> Queue;
	Queue.Append(InQueue.Queue.GetData(), BatchCount);
	TArray<UVoxelGenerator*> Generators;
	for(UVoxelGenerator* Generator : InGenerators)
	{
		if(Generator)
		{
			Generator->PrepareBatch(Queue);
			Generators.Add(Generator);
		}
	}
	const TSharedRef<FVoxelChunkQueueBatch, ESPMode::ThreadSafe> Batch = MakeShared<FVoxelChunkQueueBatch, ESPMode::ThreadSafe>(MoveTemp(Queue));
	const int32 DispatchCount = FMath::Min(WorkerCount, ChunkQueueThreads.Num());
	ActiveChunkQueueThreads.Empty(DispatchCount);
	for(int32 i = 0; i < DispatchCount; i++)
	{
		if(ChunkQueueThreads[i]->Dispatch(Batch, InFunc, InStage)) ActiveChunkQueueThreads.Add(ChunkQueueThreads[i]);
	}
	if(ActiveChunkQueueThreads.Num() == 0)
	{
		for(UVoxelGenerator* Generator : Generators) Generator->CompleteBatch(true);
		return false;
	}

	ActiveChunkQueueBatch = Batch;
	ActiveChunkQueue = &InQueue;
	ActiveChunkQueueGenerators = MoveTemp(Generators);
	return true;
}

void UVoxelModule::CancelChunkQueueBatch()
{
	if(ActiveChunkQueueBatch) ActiveChunkQueueBatch->Cancel();
	for(FVoxelChunkQueueThread* Thread : ActiveChunkQueueThreads)
	{
		if(Thread) Thread->WaitForIdle();
	}
	for(UVoxelGenerator* Generator : ActiveChunkQueueGenerators)
	{
		if(Generator) Generator->CompleteBatch(true);
	}
	ActiveChunkQueueBatch.Reset();
	ActiveChunkQueue = nullptr;
	ActiveChunkQueueThreads.Empty();
	ActiveChunkQueueGenerators.Empty();
}

void UVoxelModule::SortChunkQueue(EVoxelWorldState InState, FVoxelChunkQueue& InQueue)
{
	if(!InQueue.bSortRequired) return;
	if(InState == EVoxelWorldState::Generating)
	{
		InQueue.Queue.Sort([this](const FIndex& A, const FIndex& B)
		{
			const float DistanceA = WorldCenterIndex.DistanceTo(A, false, true);
			const float DistanceB = WorldCenterIndex.DistanceTo(B, false, true);
			if(!FMath::IsNearlyEqual(DistanceA, DistanceB)) return DistanceA < DistanceB;
			if(A.X != B.X) return A.X < B.X;
			if(A.Y != B.Y) return A.Y < B.Y;
			return A.Z < B.Z;
		});
	}
	else if(InState == EVoxelWorldState::Unloading)
	{
		InQueue.Queue.Sort([this](const FIndex& A, const FIndex& B)
		{
			const float DistanceA = WorldCenterIndex.DistanceTo(A, false, true);
			const float DistanceB = WorldCenterIndex.DistanceTo(B, false, true);
			if(!FMath::IsNearlyEqual(DistanceA, DistanceB)) return DistanceA > DistanceB;
			if(A.X != B.X) return A.X < B.X;
			if(A.Y != B.Y) return A.Y < B.Y;
			return A.Z < B.Z;
		});
	}
	InQueue.bSortRequired = false;
}

void UVoxelModule::ShutdownChunkQueueThreads()
{
	CancelChunkQueueBatch();
	for(FVoxelChunkQueueThread* Thread : ChunkQueueThreads) delete Thread;
	ChunkQueueThreads.Empty();
}

bool UVoxelModule::UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex)> InFunc)
{
	return UpdateChunkQueue(InState, [InFunc](FIndex Index, int32 Stage) { InFunc(Index); });
}

bool UVoxelModule::UpdateChunkQueue(EVoxelWorldState InState, TFunction<void(FIndex, int32)> InFunc)
{
	FVoxelChunkQueues& QueueGroup = ChunkQueues[InState];
	ITER_ARRAY_WITHINDEX(QueueGroup.Queues, i, Item,
		QueueGroup.Stage = i + 1;
		SortChunkQueue(InState, Item);
		if(Item.Queue.Num() > 0)
		{
			if(Item.bAsync && ActiveChunkQueueBatch) return true;

			const TFunction<void(FIndex, int32)> Func([this, InState, InFunc, Generators = Item.Generators](FIndex Index, int32 Stage)
			{
				if(InState == EVoxelWorldState::Spawning) InFunc(Index, Stage);
				if(UVoxelChunk* Chunk = GetChunkByIndex(Index))
				{
					for(UVoxelGenerator* Generator : Generators)
					{
						if(Generator) Generator->Generate(Chunk);
					}
				}
				if(InState != EVoxelWorldState::Spawning) InFunc(Index, Stage);
			});

			if(Item.bAsync && DispatchChunkQueue(Item, Func, i + 1, Item.Generators)) return true;

			const int32 Num = FMath::Min(FMath::Max(1, Item.Speed), Item.Queue.Num());
			TArray<FIndex> BatchIndices;
			BatchIndices.Append(Item.Queue.GetData(), Num);
			for(UVoxelGenerator* Generator : Item.Generators) if(Generator) Generator->PrepareBatch(BatchIndices);
			DON_WITHINDEX(Num, j, Func(Item.Queue[j], i + 1); )
			for(UVoxelGenerator* Generator : Item.Generators) if(Generator) Generator->CompleteBatch(false);
			Item.RemoveFront(Num);
			if(Item.bAsync || Item.Queue.Num() > 0)
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
		if((InState == EVoxelWorldState::Spawning ? !ChunkMap.Contains(InIndex) : ChunkMap.Contains(InIndex)))
		{
			Item.Add(InIndex);
		}
	)
}

void UVoxelModule::RemoveFromChunkQueue(EVoxelWorldState InState, FIndex InIndex)
{
	ITER_ARRAY(ChunkQueues[InState].Queues, Item,
		Item.Remove(InIndex);
	)
}

bool UVoxelModule::IsOnTheWorld(FIndex InIndex, bool bIgnoreZ) const
{
	const FVector2D SpawnRange = WorldData->GetWorldSize() * 0.5f;
	return InIndex.X >= WorldCenterIndex.X - SpawnRange.X && InIndex.X < WorldCenterIndex.X + SpawnRange.X &&
		InIndex.Y >= WorldCenterIndex.Y - SpawnRange.Y && InIndex.Y < WorldCenterIndex.Y + SpawnRange.Y &&
		(!bIgnoreZ || InIndex.Z >= 0 && InIndex.Z < WorldData->SkyHeight);
}

UVoxelChunk* UVoxelModule::GetChunkByIndex(FIndex InIndex) const
{
	if(ChunkMap.Contains(InIndex))
	{
		return ChunkMap[InIndex];
	}
	return nullptr;
}

void UVoxelModule::ForEachChunk(TFunctionRef<void(const UVoxelChunk&)> InVisitor) const
{
	for(const auto& Iter : ChunkMap)
	{
		if(Iter.Value) InVisitor(*Iter.Value);
	}
}

UVoxelChunk* UVoxelModule::GetChunkByLocation(FVector InLocation) const
{
	return GetChunkByIndex(LocationToChunkIndex(InLocation));
}

UVoxelChunk* UVoxelModule::GetChunkByVoxelIndex(FIndex InIndex) const
{
	return GetChunkByIndex(VoxelIndexToChunkIndex(InIndex));
}

bool UVoxelModule::HasVoxelByIndex(FIndex InIndex, bool bSafe)
{
	if(UVoxelChunk* Chunk = GetChunkByVoxelIndex(InIndex))
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
	if(UVoxelChunk* Chunk = GetChunkByVoxelIndex(InIndex))
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
	if(UVoxelChunk* Chunk = GetChunkByVoxelIndex(InIndex))
	{
		Chunk->SetVoxel(Chunk->WorldIndexToLocal(InIndex), InVoxelItem, bSafe || Chunk->IsBuilded());
	}
}

void UVoxelModule::SetVoxelByLocation(FVector InLocation, const FVoxelItem& InVoxelItem, bool bSafe)
{
	SetVoxelByIndex(LocationToVoxelIndex(InLocation), InVoxelItem, bSafe);
}

void UVoxelModule::AddToVoxelUpdateQueue(FIndex InIndex)
{
	if(!IsInGameThread())
	{
		VoxelUpdateQueue.Enqueue(InIndex);
		return;
	}
	if(UVoxelChunk* Chunk = GetChunkByVoxelIndex(InIndex))
	{
		Chunk->VoxelUpdateIndices.Add(Chunk->WorldIndexToLocal(InIndex));
		VoxelUpdateChunkIndices.Add(Chunk->GetIndex());
	}
}

void UVoxelModule::AddToVoxelLiquidUpdateQueue(FIndex InIndex)
{
	if(!VoxelLiquidUpdateIndices.Contains(InIndex))
	{
		VoxelLiquidUpdateIndices.Add(InIndex);
		VoxelLiquidUpdateQueue.Enqueue(InIndex);
	}
}

void UVoxelModule::UpdateVoxelQueue()
{
	TArray<FIndex> ChunkIndices = VoxelUpdateChunkIndices.Array();
	TSet<FIndex> LiquidUpdateIndexSet;
	FIndex LiquidUpdateIndex;
	const int32 LiquidUpdateCount = VoxelLiquidUpdateIndices.Num();
	while(LiquidUpdateIndexSet.Num() < LiquidUpdateCount && VoxelLiquidUpdateQueue.Dequeue(LiquidUpdateIndex))
	{
		VoxelLiquidUpdateIndices.Remove(LiquidUpdateIndex);
		LiquidUpdateIndexSet.Add(LiquidUpdateIndex);
	}
	TArray<FIndex> LiquidUpdateIndices = LiquidUpdateIndexSet.Array();
	TSet<FIndex> ChangedChunkIndices;
	int32 RemainingUpdates = 256;
	for(const FIndex& ChunkIndex : ChunkIndices)
	{
		UVoxelChunk* Chunk = GetChunkByIndex(ChunkIndex);
		if(!Chunk)
		{
			VoxelUpdateChunkIndices.Remove(ChunkIndex);
			continue;
		}
		if(!Chunk->IsGenerated()) continue;

		RemainingUpdates -= Chunk->UpdateVoxels(FMath::Min(RemainingUpdates, 32), ChangedChunkIndices);
		if(Chunk->VoxelUpdateIndices.IsEmpty()) VoxelUpdateChunkIndices.Remove(ChunkIndex);
		if(RemainingUpdates <= 0) break;
	}

	for(const FIndex& ChunkIndex : ChangedChunkIndices)
	{
		if(UVoxelChunk* Chunk = GetChunkByIndex(ChunkIndex); Chunk && Chunk->IsGenerated()) Chunk->Generate(EPhase::Lesser);
	}
	if(LiquidUpdateIndices.IsEmpty()) return;

	TSet<FIndex> LiquidEvaluationIndexSet = LiquidUpdateIndexSet;
	for(const FIndex& Index : LiquidUpdateIndices)
	{
		ITER_DIRECTION(Direction, LiquidEvaluationIndexSet.Add(Index + FMathHelper::DirectionToIndex(Direction)); )
		for(const EDirectionN Iter : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left })
		{
			LiquidEvaluationIndexSet.Add(Index + FMathHelper::DirectionToIndex(Iter) + FIndex(0, 0, -1));
		}
	}
	TMap<FIndex, FVoxelLiquidSnapshot> LiquidSnapshots;
	for(const FIndex& Index : LiquidEvaluationIndexSet)
	{
		TSet<FIndex> SnapshotIndices;
		SnapshotIndices.Add(Index);
		ITER_DIRECTION(Direction, SnapshotIndices.Add(Index + FMathHelper::DirectionToIndex(Direction)); )
		SnapshotIndices.Add(Index + FIndex(0, 0, -2));
		for(const EDirectionN Iter : { EDirectionN::Forward, EDirectionN::Right, EDirectionN::Backward, EDirectionN::Left })
		{
			const FIndex NeighborIndex = Index + FMathHelper::DirectionToIndex(Iter);
			SnapshotIndices.Add(NeighborIndex + FIndex(0, 0, 1));
			SnapshotIndices.Add(NeighborIndex + FIndex(0, 0, -1));
			SnapshotIndices.Add(NeighborIndex + FIndex(0, 0, -2));
		}
		for(const FIndex& SnapshotIndex : SnapshotIndices)
		{
			if(LiquidSnapshots.Contains(SnapshotIndex)) continue;
			FVoxelLiquidSnapshot Snapshot;
			if(const UVoxelChunk* Chunk = GetChunkByVoxelIndex(SnapshotIndex); Chunk && Chunk->IsGenerated())
			{
				Snapshot.bGenerated = true;
				const FVoxelItem& Item = GetVoxelByIndex(SnapshotIndex);
				Snapshot.VoxelType = Item.IsUnknown() ? EVoxelType::Unknown : Item.IsValid() ? Item.GetVoxelType() : EVoxelType::Empty;
				Snapshot.Data = Item.Data;
				Snapshot.bCanFlowThrough = Item.IsValid() && (Item.GetData().Nature == EVoxelNature::Foliage || Item.GetData().Nature == EVoxelNature::SemiFoliage);
			}
			LiquidSnapshots.Add(SnapshotIndex, MoveTemp(Snapshot));
		}
	}

	bVoxelUpdateRunning = true;
	TWeakObjectPtr<UVoxelModule> Module(this);
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Module, LiquidEvaluationIndices = LiquidEvaluationIndexSet.Array(), LiquidSnapshots = MoveTemp(LiquidSnapshots)]() mutable
	{
		TMap<FIndex, FVoxelLiquidUpdate> LiquidUpdates;
		for(const FIndex& Index : LiquidEvaluationIndices)
		{
			FVoxelLiquidUpdate Update;
			if(UVoxelModuleStatics::CalculateVoxelLiquidUpdate(Index, LiquidSnapshots, Update)) LiquidUpdates.Add(Index, MoveTemp(Update));
		}
		AsyncTask(ENamedThreads::GameThread, [Module, LiquidSnapshots = MoveTemp(LiquidSnapshots), LiquidUpdates = MoveTemp(LiquidUpdates)]() mutable
		{
			if(!Module.IsValid()) return;
			Module->bVoxelUpdateRunning = false;
			if(Module->WorldMode == EVoxelWorldMode::None) return;
			TMap<FIndex, FVoxelItem> VoxelUpdates;
			TSet<FIndex> VegetationChangedChunkIndices;
			for(const auto& Iter : LiquidUpdates)
			{
				const FVoxelLiquidSnapshot* Snapshot = LiquidSnapshots.Find(Iter.Key);
				if(!Snapshot || !Snapshot->bGenerated) continue;
				const FVoxelItem& CurrentItem = Module->GetVoxelByIndex(Iter.Key);
				const EVoxelType CurrentType = CurrentItem.IsUnknown() ? EVoxelType::Unknown : CurrentItem.IsValid() ? CurrentItem.GetVoxelType() : EVoxelType::Empty;
				if(CurrentType != Snapshot->VoxelType || CurrentItem.Data != Snapshot->Data)
				{
					Module->AddToVoxelLiquidUpdateQueue(Iter.Key);
					continue;
				}
				if(Iter.Value.bRemove)
				{
					VoxelUpdates.Add(Iter.Key, FVoxelItem::Empty);
				}
				else
				{
					if(Snapshot->bCanFlowThrough)
					{
						if(UVoxelModuleStatics::GetVoxelWorldMode() != EVoxelWorldMode::Prefab)
						{
							const UVoxelData& VoxelData = CurrentItem.GetData();
							UAbilityModuleStatics::SpawnAbilityPickUp(FAbilityItem(VoxelData.GatherData ? VoxelData.GatherData->GetPrimaryAssetId() : VoxelData.GetPrimaryAssetId(), 1),
								CurrentItem.GetLocation() + VoxelData.GetRange(CurrentItem.Angle) * Module->GetWorldData().BlockSize * 0.5f, CurrentItem.Chunk);
						}
						VegetationChangedChunkIndices.Add(CurrentItem.Chunk->GetIndex());
					}
					FVoxelItem Item = CurrentItem;
					if(CurrentType != EVoxelType::Water) Item = FVoxelItem(EVoxelType::Water);
					Item.Data = Iter.Value.Data;
					VoxelUpdates.Add(Iter.Key, Item);
				}
			}
			TSet<FIndex> ChangedChunkIndices;
			Module->ApplyVoxelUpdates(VoxelUpdates, ChangedChunkIndices);
			for(const auto& Iter : VoxelUpdates) Module->AddToVoxelLiquidUpdateQueue(Iter.Key);
			for(const FIndex& ChunkIndex : ChangedChunkIndices)
			{
				if(UVoxelChunk* Chunk = Module->GetChunkByIndex(ChunkIndex))
				{
					Chunk->BuildMesh(EVoxelNature::Liquid);
					Chunk->CreateMesh(EVoxelNature::Liquid);
				}
			}
			for(const FIndex& ChunkIndex : VegetationChangedChunkIndices)
			{
				if(UVoxelChunk* Chunk = Module->GetChunkByIndex(ChunkIndex))
				{
					Chunk->BuildMesh(EVoxelNature::Foliage);
					Chunk->CreateMesh(EVoxelNature::Foliage);
					Chunk->BuildMesh(EVoxelNature::SemiFoliage);
					Chunk->CreateMesh(EVoxelNature::SemiFoliage);
				}
			}
		});
	});
}

void UVoxelModule::ApplyVoxelUpdates(const TMap<FIndex, FVoxelItem>& InVoxelMap, TSet<FIndex>& OutChangedChunkIndices)
{
	for(const auto& Iter : InVoxelMap)
	{
		if(UVoxelChunk* Chunk = GetChunkByVoxelIndex(Iter.Key); Chunk && Chunk->IsGenerated())
		{
			const FIndex LocalIndex = Chunk->WorldIndexToLocal(Iter.Key);
			const FVoxelItem& CurrentItem = Chunk->GetVoxel(LocalIndex);
			if(CurrentItem.ID == Iter.Value.ID && CurrentItem.Data == Iter.Value.Data) continue;
			Chunk->SetVoxel(LocalIndex, Iter.Value, true);
			Chunk->SetChanged(true);
			for(int32 X = -1; X <= 1; ++X)
			{
				for(int32 Y = -1; Y <= 1; ++Y)
				{
					if(UVoxelChunk* ChangedChunk = GetChunkByVoxelIndex(Iter.Key + FIndex(X, Y, 0)); ChangedChunk && ChangedChunk->IsGenerated())
					{
						OutChangedChunkIndices.Add(ChangedChunk->GetIndex());
					}
				}
			}
		}
	}
}

const FVoxelTopography& UVoxelModule::GetTopographyByIndex(FIndex InIndex)
{
	if(UVoxelChunk* Chunk = GetChunkByVoxelIndex(FIndex(InIndex.X, InIndex.Y, 0)))
	{
		return Chunk->GetTopography(Chunk->WorldIndexToLocal(FIndex(InIndex.X, InIndex.Y, 0)));
	}
	static FVoxelTopography Temp;
	return Temp;
}

const FVoxelTopography& UVoxelModule::GetTopographyByLocation(FVector InLocation)
{
	return GetTopographyByIndex(LocationToVoxelIndex(InLocation));
}

void UVoxelModule::SetTopographyByIndex(FIndex InIndex, const FVoxelTopography& InTopography)
{
	if(UVoxelChunk* Chunk = GetChunkByVoxelIndex(FIndex(InIndex.X, InIndex.Y, 0)))
	{
		if(!Chunk->IsBuilded())
		{
			Chunk->SetTopography(Chunk->WorldIndexToLocal(FIndex(InIndex.X, InIndex.Y, 0)), InTopography);
		}
	}
}

void UVoxelModule::SetTopographyByLocation(FVector InLocation, const FVoxelTopography& InTopography)
{
	SetTopographyByIndex(LocationToVoxelIndex(InLocation), InTopography);
}

FVoxelTopography UVoxelModule::SampleBaseTopographyByIndex(FIndex InIndex) const
{
	if(const UVoxelSurfaceGenerator* Generator = GetVoxelGenerator<UVoxelSurfaceGenerator>()) return Generator->SampleTopography(InIndex);
	return FVoxelTopography();
}

FVoxelTopography UVoxelModule::SampleTopographyByIndex(FIndex InIndex) const
{
	FVoxelTopography Topography = SampleBaseTopographyByIndex(InIndex);
	if(const UVoxelRiverGenerator* Generator = GetVoxelGenerator<UVoxelRiverGenerator>())
	{
		Generator->ApplyToTopography(InIndex, Topography);
	}
	if(const UVoxelLakeGenerator* Generator = GetVoxelGenerator<UVoxelLakeGenerator>())
	{
		Generator->ApplyToTopography(InIndex, Topography);
	}
	return Topography;
}

EVoxelRegionType UVoxelModule::GetWorldRegionByIndex(FIndex InIndex) const
{
	return SampleTopographyByIndex(InIndex).RegionType;
}

float UVoxelModule::GetVoxelNoise1D(float InValue, bool bAbs, bool bUnsigned) const
{
	return FMathHelper::GetNoise1D(InValue, WorldData->WorldSeed, bAbs, bUnsigned);
}

float UVoxelModule::GetVoxelNoise2D(FVector2D InLocation, bool bAbs, bool bUnsigned) const
{
	return FMathHelper::GetNoise2D(InLocation, WorldData->WorldSeed, bAbs, bUnsigned);
}

float UVoxelModule::GetVoxelNoise3D(FVector InLocation, bool bAbs, bool bUnsigned) const
{
	return FMathHelper::GetNoise3D(InLocation, WorldData->WorldSeed, bAbs, bUnsigned);
}

FIndex UVoxelModule::LocationToChunkIndex(FVector InLocation) const
{
	return FIndex(FMath::FloorToInt(InLocation.X / WorldData->GetChunkRealSize().X), FMath::FloorToInt(InLocation.Y / WorldData->GetChunkRealSize().Y), 0);
}

FVector UVoxelModule::ChunkIndexToLocation(FIndex InIndex) const
{
	return InIndex.ToVector() * FVector(WorldData->GetChunkRealSize().X, WorldData->GetChunkRealSize().Y, 0.f);
}

FIndex UVoxelModule::ChunkIndexToVoxelIndex(FIndex InIndex) const
{
	return InIndex * WorldData->ChunkSize;
}

FIndex UVoxelModule::LocationToVoxelIndex(FVector InLocation) const
{
	InLocation /= WorldData->BlockSize;
	return FIndex(FMath::FloorToInt(InLocation.X), FMath::FloorToInt(InLocation.Y), FMath::FloorToInt(InLocation.Z));
}

FVector UVoxelModule::VoxelIndexToLocation(FIndex InIndex) const
{
	return InIndex.ToVector() * WorldData->BlockSize;
}

FIndex UVoxelModule::VoxelIndexToChunkIndex(FIndex InIndex) const
{
	const FVector2D Index = InIndex.ToVector2D() / WorldData->ChunkSize;
	return FIndex(FMath::FloorToInt(Index.X), FMath::FloorToInt(Index.Y), 0);
}

uint64 UVoxelModule::VoxelIndexToNumber(FIndex InIndex, bool bWorldSpace) const
{
	if(!bWorldSpace)
	{
		const int32 SizeX = (int32)WorldData->ChunkSize.X;
		return InIndex.X + InIndex.Y * SizeX + InIndex.Z * SizeX * (int32)WorldData->ChunkSize.Y;
	}
	return FMathHelper::CompressIndex(InIndex);
}

FIndex UVoxelModule::NumberToVoxelIndex(uint64 InNumber, bool bWorldSpace) const
{
	if(!bWorldSpace)
	{
		const int32 Num1 = (int32)WorldData->ChunkSize.X;
		const int32 Num2 = Num1 * (int32)WorldData->ChunkSize.Y;
		const int32 Num3 = InNumber % Num2;
		return FIndex(Num3 % Num1, Num3 / Num1, InNumber / Num2);
	}
	return FMathHelper::UnCompressIndex(InNumber);
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
	if(AWHPlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
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
	const FVector Size = InVoxelItem.GetRange(true, true) * WorldData->BlockSize * 0.5f;
	const FVector Location = InVoxelItem.GetLocation();
	return UKismetSystemLibrary::BoxTraceSingle(GetWorldContext(), Location + Size, Location + Size, Size * 0.95f, FRotator::ZeroRotator, USceneModuleStatics::GetTraceMapping(FName("Voxel")).GetTraceType(), false, InIgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
}

bool UVoxelModule::VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter, bool bForce)
{
	const FVector2D ChunkRadius = WorldData->GetChunkRealSize() * 0.5f;
	const FVector ChunkLocation = ChunkIndexToLocation(InChunkIndex);
	return VoxelAgentTraceSingle(ChunkLocation + FVector(ChunkRadius.X, ChunkRadius.Y, 0.f), FVector2D(ChunkRadius.X, ChunkRadius.Y), InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter, bForce);
}

bool UVoxelModule::VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter, bool bForce)
{
	const FBox WorldBounds = GetWorldBounds(InRadius, InHalfHeight);
	InLocation.X = FMath::Clamp(InLocation.X, WorldBounds.Min.X, WorldBounds.Max.X);
	InLocation.Y = FMath::Clamp(InLocation.Y, WorldBounds.Min.Y, WorldBounds.Max.Y);
	DON_WITHINDEX(InMaxCount, i,
		FVector RayStart = FVector((bFromCenter && i == 0) ? 0.f : WorldData->RandomStream.FRandRange(-InRange.X * 0.5f, InRange.X * 0.5f),
			(bFromCenter && i == 0) ? 0.f : WorldData->RandomStream.FRandRange(-InRange.Y * 0.5f, InRange.Y * 0.5f), WorldData->GetWorldRealHeight());
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

bool UVoxelModule::IsChunkGenerated(FIndex InIndex) const
{
	if(UVoxelChunk* Chunk = GetChunkByIndex(InIndex))
	{
		return Chunk->IsGenerated();
	}
	return false;
}

FVoxelChunkQueues UVoxelModule::GetChunkQueues(EVoxelWorldState InWorldState) const
{
	return ChunkQueues.FindRef(InWorldState);
}

UVoxelGenerator* UVoxelModule::GetVoxelGenerator(const TSubclassOf<UVoxelGenerator>& InClass) const
{
	return VoxelGeneratorMap.FindRef(InClass);
}

FPrimaryAssetId UVoxelModule::VoxelTypeToAssetID(EVoxelType InVoxelType) const
{
	if(VoxelAssetIDMap.Contains(InVoxelType))
	{
		return VoxelAssetIDMap[InVoxelType];
	}
	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_%s"), *UCommonModuleStatics::GetEnumAuthoredNameByValue(TEXT("/Script/WHFramework.EVoxelType"), (int32)InVoxelType)));
}

FSceneArea UVoxelModule::ResolveVoxelArea(const FSceneArea& InArea, const FVector2D& InPoint) const
{
	const float BlockSize = FMath::Max(GetWorldData().BlockSize, UE_SMALL_NUMBER);
	const FIndex Index(FMath::FloorToInt(InPoint.X / BlockSize), FMath::FloorToInt(InPoint.Y / BlockSize), 0);
	const FText RegionDisplayName = UCommonModuleStatics::GetEnumDisplayNameByValue(TEXT("/Script/WHFramework.EVoxelRegionType"), static_cast<int32>(SampleTopographyByIndex(Index).RegionType));
	FSceneArea Area = InArea;
	Area.AreaDisplayName = InArea.AreaDisplayName.IsEmpty()
		? GetVoxelAreaName(Index, EVoxelAreaType::Continent, RegionDisplayName)
		: FText::Format(FText::FromString(TEXT("{0}{1}")), InArea.AreaDisplayName, RegionDisplayName);
	return Area;
}

FText UVoxelModule::GetVoxelAreaName(FIndex InIndex) const
{
	if(IsInGameThread())
	{
		const FSceneArea Area = USceneModule::Get().GetSceneAreaByPoint(InIndex.ToVector2D() * GetWorldData().BlockSize);
		if(!Area.AreaName.IsNone() && !Area.AreaDisplayName.IsEmpty()) return Area.AreaDisplayName;
	}
	const FText RegionDisplayName = UCommonModuleStatics::GetEnumDisplayNameByValue(TEXT("/Script/WHFramework.EVoxelRegionType"), static_cast<int32>(SampleTopographyByIndex(InIndex).RegionType));
	return GetVoxelAreaName(InIndex, EVoxelAreaType::Continent, RegionDisplayName);
}

FText UVoxelModule::GetVoxelAreaName(FIndex InIndex, EVoxelAreaType InAreaType) const
{
	const int32 WorldSeed = WorldData ? WorldData->WorldSeed : 0;
	const uint32 Hash = HashCombine(HashCombine(GetTypeHash(WorldSeed), GetTypeHash(InIndex.X)), HashCombine(GetTypeHash(InIndex.Y), GetTypeHash(static_cast<uint8>(InAreaType))));
	const FString Prefix = StaticEnum<EVoxelAreaType>()->GetNameStringByValue(static_cast<int64>(InAreaType)) + TEXT(".");
	const FName Key = UAssetModuleStatics::GetRandomTextKey(VoxelAreaNamespace, Prefix, Hash);
	return Key.IsNone() ? FText::GetEmpty() : UAssetModuleStatics::GetLocalizedText(VoxelAreaNamespace, Key.ToString());
}

FText UVoxelModule::GetVoxelAreaName(FIndex InIndex, EVoxelAreaType InAreaType, const FText& InAreaName) const
{
	const FText Prefix = GetVoxelAreaName(InIndex, InAreaType);
	if(Prefix.IsEmpty()) return InAreaName;
	if(InAreaName.IsEmpty()) return Prefix;

	return FText::Format(FText::FromString(TEXT("{0}{1}")), Prefix, InAreaName);
}
