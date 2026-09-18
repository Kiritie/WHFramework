#include "SaveGame/SaveGameModule.h"

#include "Event/EventModuleStatics.h"
#include "Event/Events/Common/Game/Event_GameExited.h"
#include "Gameplay/WHGameInstance.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveDataSerializer.h"
#include "SaveGame/SaveGameStorage.h"
#include "SaveGame/SaveGameAsyncExecutor.h"
#include "Setting/SettingModule.h"
#include "Scene/SceneModule.h"
#include "Scene/Object/WorldTimer.h"
#include "Scene/Object/WorldWeather.h"
#include "Voxel/VoxelModule.h"
#include "EngineUtils.h"

IMPLEMENTATION_MODULE(USaveGameModule)

namespace
{
void RestoreSceneEnvironment(UModuleBase* Module, const FParameter& Data, EPhase Phase)
{
	USceneModule* Scene = Cast<USceneModule>(Module);
	const FSceneModuleSaveData* SceneData = Data.GetPtr<FSceneModuleSaveData>();
	if(!Scene || !SceneData) return;

	// Keep timer/weather restoration explicit at the save transaction boundary.
	// This also repairs projects that temporarily shipped SceneModule with these
	// two agent-load calls commented out. Reapplying the same values is safe.
	if(UWorldTimer* Timer = Scene->GetWorldTimer())
	{
		if(Timer->IsAutoSave()) Timer->LoadSaveData(FParameter(SceneData->TimerData), Phase);
		Timer->OnRefresh(0.f);
	}
	if(UWorldWeather* Weather = Scene->GetWorldWeather())
	{
		if(Weather->IsAutoSave()) Weather->LoadSaveData(FParameter(SceneData->WeatherData), Phase);
		Weather->OnRefresh(0.f);
	}
}
}

USaveGameModule::USaveGameModule()
{
	ModuleName = FName("SaveGameModule");
	ModuleDisplayName = FText::FromString(TEXT("Save Game Module"));
	bModuleRequired = true;
	SaveScope = ESaveScope::None;
	UserIndex = 0;
	bSaveOperationRunning = false;
	bHasPendingSaveSlot = false;
	PendingSaveSlotParams = FCreateSaveSlotParams();
}

USaveGameModule::~USaveGameModule()
{
	TERMINATION_MODULE(USaveGameModule)
}

#if WITH_EDITOR
void USaveGameModule::OnDestroy()
{
	Super::OnDestroy();
	TERMINATION_MODULE(USaveGameModule)
}
#endif

void USaveGameModule::RestoreLastActiveSave()
{
	ActiveSaveId.Invalidate();
	if(!Storage)
	{
		return;
	}

	if(Storage->ReadLastActiveSave(ActiveSaveId))
	{
		return;
	}

	// Migrate installs created before last_active.txt existed. Only complete,
	// committed saves are exposed by GetSaveSlotSummaries().
	Storage->ClearLastActiveSave();
	const TArray<FSaveSlotSummary> Saves = GetSaveSlotSummaries();
	if(!Saves.IsEmpty())
	{
		ActiveSaveId = Saves[0].SaveId;
		if(!Storage->WriteLastActiveSave(ActiveSaveId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not persist the recovered last active save."));
		}
	}
}

void USaveGameModule::OnInitialize()
{
	Super::OnInitialize();
	Storage = MakeUnique<FSaveGameStorage>(UserIndex);
	Storage->EnsureRoot();
	Storage->CleanupAllTempGenerations();
	RestoreLastActiveSave();
	if (UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		PendingLoadContext = GameInstance->ConsumePendingSaveLoad();
	}
	UEventModuleStatics::SubscribeEvent<FEventGameExited>(this, &ThisClass::OnGameExited);
}

void USaveGameModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	if (PHASEC(InPhase, EPhase::Final) && PendingLoadContext.IsValid())
	{
		const FPendingSaveLoadContext Context = PendingLoadContext;
		PendingLoadContext = FPendingSaveLoadContext();
		RestoreSlotGeneration(Context, Context.LoadPhase);
	}
}

void USaveGameModule::OnTermination(EPhase InPhase)
{
	FinishPendingSave();
	Super::OnTermination(InPhase);
	if (PHASEC(InPhase, EPhase::Final))
	{
		AsyncSave.Reset();
		Storage.Reset();
	}
}

void USaveGameModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
	if (bInEditor || !AsyncSave)
	{
		return;
	}
	FSaveOperationResult Result = FSaveOperationResult::Success();
	if (AsyncSave->Poll(Result))
	{
		CompleteAsyncSave(Result);
	}
}

void USaveGameModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
}

FParameter USaveGameModule::ToData()
{
	return FParameter();
}

void USaveGameModule::SetUserIndex(int32 InUserIndex)
{
	if (bSaveOperationRunning)
	{
		return;
	}
	DetachActiveSave();
	UserIndex = InUserIndex;
	Storage = MakeUnique<FSaveGameStorage>(UserIndex);
	Storage->EnsureRoot();
	Storage->CleanupAllTempGenerations();
	RestoreLastActiveSave();
}

FSaveOperationResult USaveGameModule::CreateSaveSlot(const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary)
{
	if (!Storage || bSaveOperationRunning || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Cannot create world slot now")));
	}

	const FGuid PreviousActiveSaveId = ActiveSaveId;
	FGuid PreviousPersistedSaveId;
	Storage->ReadLastActiveSave(PreviousPersistedSaveId);
	FSaveManifest Manifest;
	Manifest.SaveId = FGuid::NewGuid();
	Manifest.DisplayName = Params.DisplayName;
	Manifest.Description = Params.Description;
	Manifest.CreatedAt = FDateTime::UtcNow();
	Manifest.UpdatedAt = Manifest.CreatedAt;
	Manifest.CurrentMap = Params.InitialMap;

	{
		TGuardValue<bool> Guard(bSaveOperationRunning, true);
		if (!Storage->CreateWorldDirectory(Manifest.SaveId) || !Storage->WriteManifestAtomic(Manifest.SaveId, Manifest))
		{
			Storage->DeleteWorldDirectory(Manifest.SaveId);
			return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Create save slot failed")));
		}
	}

	if (Params.bCaptureCurrentWorld)
	{
		const FSaveOperationResult Result = SaveSlot(Manifest.SaveId);
		if (!Result)
		{
			Storage->DeleteWorldDirectory(Manifest.SaveId);
			ActiveSaveId = PreviousActiveSaveId;
			if(PreviousPersistedSaveId.IsValid()) Storage->WriteLastActiveSave(PreviousPersistedSaveId);
			else Storage->ClearLastActiveSave();
			return Result;
		}
		if (!Storage->ReadManifest(Manifest.SaveId, Manifest))
		{
			Storage->DeleteWorldDirectory(Manifest.SaveId);
			ActiveSaveId = PreviousActiveSaveId;
			if(PreviousPersistedSaveId.IsValid()) Storage->WriteLastActiveSave(PreviousPersistedSaveId);
			else Storage->ClearLastActiveSave();
			return FSaveOperationResult::Failed(ESaveResultCode::ReadFailed, FText::FromString(TEXT("Committed manifest could not be reread")));
		}
	}
	else
	{
		ActiveSaveId = Manifest.SaveId;
	}

	OutSummary = Manifest.ToSummary(Storage->GetWorldDir(Manifest.SaveId));
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::SaveSlot(FGuid SaveId)
{
	const FSaveOperationResult Started = SaveSlotAsync(SaveId);
	return Started ? FinishPendingSave() : Started;
}

FSaveOperationResult USaveGameModule::SaveActiveSlot()
{
	return ActiveSaveId.IsValid() ? SaveSlot(ActiveSaveId) : FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("No active save")));
}

FSaveOperationResult USaveGameModule::SaveSlotAsync(FGuid SaveId)
{
	check(IsInGameThread());
	if (!Storage || bSaveOperationRunning || !SaveId.IsValid() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("World save is unavailable or busy")));
	}
	FSaveGenerationPlan Plan;
	Plan.SaveId = SaveId;
	Plan.UserIndex = UserIndex;
	if (!Storage->ReadManifest(SaveId, Plan.Manifest) || Plan.Manifest.CurrentGeneration == MAX_int32)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("Save slot not found")));
	}
	Plan.CurrentMap = FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
	bSaveOperationRunning = true;
	AsyncSaveId = SaveId;
	AsyncCommittedDirectory = Storage->GetGenerationDir(SaveId, Plan.Manifest.CurrentGeneration + 1);
	AsyncCaptured.Reset();
	const TArray<UModuleBase*> Modules = GetSaveModules(ESaveScope::World);
	for (UModuleBase* Module : Modules)
	{
		AsyncCaptured.Add(Module);
		Module->OnBeforeSaveData();
	}
	for (UModuleBase* Module : Modules)
	{
		FSaveModuleBytes Item;
		Item.ModuleName = Module->GetModuleName();
		Item.Version = Module->GetSaveDataVersion();
		if (!Module->BuildSaveFile(Item.Bytes))
		{
			const FSaveOperationResult Result = FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromName(Item.ModuleName));
			CompleteAsyncSave(Result);
			return Result;
		}
		Plan.Modules.Add(MoveTemp(Item));
		if (UVoxelModule* Voxel = Cast<UVoxelModule>(Module))
		{
			FVoxelModuleSaveCapture Capture;
			FString Error;
			if (!Voxel->CopySaveCapture(Capture, Error))
			{
				const FSaveOperationResult Result = FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromString(Error));
				CompleteAsyncSave(Result);
				return Result;
			}
			Voxel->SetPendingCommitDirectory(AsyncCommittedDirectory);
			Plan.Voxel = MoveTemp(Capture);
		}
	}
	if (!AsyncSave)
	{
		AsyncSave = MakeUnique<FSaveGameAsyncExecutor>();
	}
	if (!AsyncSave->Start(MoveTemp(Plan)))
	{
		const FSaveOperationResult Result = FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save worker is busy")));
		CompleteAsyncSave(Result);
		return Result;
	}
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::SaveActiveSlotAsync()
{
	return ActiveSaveId.IsValid() ? SaveSlotAsync(ActiveSaveId)
	                              : FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("No active save")));
}

void USaveGameModule::CompleteAsyncSave(const FSaveOperationResult& Result)
{
	check(IsInGameThread());
	const FGuid CompletedId = AsyncSaveId;
	TArray<TWeakObjectPtr<UModuleBase>> Captured = MoveTemp(AsyncCaptured);
	AsyncCaptured.Reset();
	AsyncSaveId.Invalidate();
	AsyncCommittedDirectory.Reset();
	for (TWeakObjectPtr<UModuleBase>& Weak : Captured)
	{
		if (UModuleBase* Module = Weak.Get())
		{
			Module->OnAfterSaveData(bool(Result));
		}
	}
	if (Result)
	{
		ActiveSaveId = CompletedId;
		if(Storage && !Storage->WriteLastActiveSave(CompletedId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Save committed, but last_active.txt could not be updated."));
		}
	}
	bSaveOperationRunning = false;
	OnWorldSaveFinished.Broadcast(CompletedId, Result);
}

FSaveOperationResult USaveGameModule::FinishPendingSave()
{
	if (!AsyncSave || !AsyncSave->IsRunning())
	{
		return FSaveOperationResult::Success();
	}
	const FSaveOperationResult Result = AsyncSave->Finish();
	CompleteAsyncSave(Result);
	return Result;
}

void USaveGameModule::BeginPendingSaveSlot(const FCreateSaveSlotParams& Params)
{
	PendingSaveSlotParams = Params;
	PendingSaveSlotParams.bCaptureCurrentWorld = true;
	bHasPendingSaveSlot = true;
}

void USaveGameModule::CancelPendingSaveSlot()
{
	bHasPendingSaveSlot = false;
	PendingSaveSlotParams = FCreateSaveSlotParams();
}

FSaveOperationResult USaveGameModule::SaveCurrentSlot()
{
	if (bHasPendingSaveSlot)
	{
		FSaveSlotSummary Summary;
		const FSaveOperationResult Result = CreateSaveSlot(PendingSaveSlotParams, Summary);
		if (Result)
		{
			CancelPendingSaveSlot();
		}
		return Result;
	}
	if (ActiveSaveId.IsValid())
	{
		return SaveActiveSlot();
	}
	return FSaveOperationResult::Success();
}

void USaveGameModule::DetachActiveSave()
{
	ActiveSaveId.Invalidate();
	if (UVoxelModule* Voxel = UVoxelModule::GetPtr())
	{
		Voxel->SetActiveSaveSource(FGuid(), 0, nullptr);
	}
}

void USaveGameModule::ClearActiveSave()
{
	DetachActiveSave();
	if(Storage)
	{
		Storage->ClearLastActiveSave();
	}
}

FSaveOperationResult USaveGameModule::LoadSlot(FGuid SaveId, EPhase InPhase)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument,
		                                    FText::FromString(TEXT("A guest cannot load a local world into the server session")));
	}
	if (!SaveId.IsValid())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save id.")));
	}
	if (!Storage || bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save system is busy.")));
	}
	TGuardValue<bool> Guard(bSaveOperationRunning, true);
	FSaveManifest Manifest;
	if (!Storage->ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	    !Storage->IsGenerationComplete(SaveId, Manifest.CurrentGeneration, Manifest))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("Committed save slot not found.")));
	}
	const FName CurrentMap(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
	if (!Manifest.CurrentMap.IsNone() && Manifest.CurrentMap != CurrentMap)
	{
		PendingLoadContext.SaveId = SaveId;
		PendingLoadContext.Generation = Manifest.CurrentGeneration;
		PendingLoadContext.TargetMap = Manifest.CurrentMap;
		PendingLoadContext.LoadPhase = InPhase;
		if (UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
		{
			GameInstance->SetPendingSaveLoad(PendingLoadContext);
		}
		UGameplayStatics::OpenLevel(this, Manifest.CurrentMap);
		return FSaveOperationResult::Success();
	}
	FPendingSaveLoadContext Context;
	Context.SaveId = SaveId;
	Context.Generation = Manifest.CurrentGeneration;
	Context.TargetMap = Manifest.CurrentMap.IsNone() ? CurrentMap : Manifest.CurrentMap;
	Context.LoadPhase = InPhase;
	return RestoreSlotGeneration(Context, InPhase);
}

FSaveOperationResult USaveGameModule::RestoreSlotGeneration(const FPendingSaveLoadContext& Context, EPhase InPhase)
{
	FSaveOperationResult Result = LoadModulesFromGeneration(Context.SaveId, Context.Generation, InPhase);
	if (Result)
	{
		ActiveSaveId = Context.SaveId;
		if(Storage && !Storage->WriteLastActiveSave(Context.SaveId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Loaded save, but last_active.txt could not be updated."));
		}
		CancelPendingSaveSlot();
	}
	return Result;
}

FSaveOperationResult USaveGameModule::LoadModulesFromGeneration(const FGuid& SaveId, int32 Generation, EPhase InPhase)
{
	struct FLoadedModuleSaveData
	{
		UModuleBase* Module = nullptr;
		FModuleSaveFileHeader Header;
		FParameter Data;
	};
	TArray<FLoadedModuleSaveData> Loaded;
	for (UModuleBase* Module : GetSaveModules(ESaveScope::World))
	{
		TArray<uint8> Bytes;
		FLoadedModuleSaveData Item;
		Item.Module = Module;
		if (!Storage->ReadBinary(Storage->GetModuleFilePath(SaveId, Generation, Module->GetModuleName()), Bytes))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::ReadFailed, FText::FromName(Module->GetModuleName()));
		}
		if (!FSaveDataSerializer::ReadModuleFile(Bytes, Item.Header, Item.Data) || Item.Header.ModuleName != Module->GetModuleName())
		{
			return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromName(Module->GetModuleName()));
		}
		if (Item.Header.ModuleVersion != Module->GetSaveDataVersion() && !Module->MigrateSaveData(Item.Header.ModuleVersion, Item.Data))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::VersionMismatch, FText::FromName(Module->GetModuleName()));
		}
		Loaded.Add(MoveTemp(Item));
	}
	for (const FLoadedModuleSaveData& Item : Loaded)
	{
		if (const UVoxelModule* Voxel = Cast<UVoxelModule>(Item.Module))
		{
			FString Error;
			if (!Voxel->ValidateWorldData(Item.Data, Error))
			{
				return FSaveOperationResult::Failed(ESaveResultCode::VersionMismatch, FText::FromString(Error));
			}
		}
	}
	if (PHASEC(InPhase, EPhase::Primary))
	{
		if (UVoxelModule* Voxel = UVoxelModule::GetPtr())
		{
			Voxel->SetActiveSaveSource(SaveId, Generation, Storage.Get());
		}
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->OnBeforeLoadData();
		}
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Primary);
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Primary);
		}
	}
	if (PHASEC(InPhase, EPhase::Lesser))
	{
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Lesser);
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Lesser);
		}
	}
	if (PHASEC(InPhase, EPhase::Final))
	{
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Final);
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Final);
		}
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->OnAfterLoadData(true);
		}
	}
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::DeleteSaveSlot(FGuid SaveId)
{
	if (bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	if (!Storage || !SaveId.IsValid())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save id.")));
	}

	FSaveManifest ExistingManifest;
	Storage->ReadManifest(SaveId, ExistingManifest);
	const bool bWasActive = SaveId == ActiveSaveId;
	FGuid PersistedSaveId;
	const bool bWasPersistedLast = Storage->ReadLastActiveSave(PersistedSaveId) && PersistedSaveId == SaveId;
	if(bWasActive)
	{
		DetachActiveSave();
	}

	if(!Storage->DeleteWorldDirectory(SaveId))
	{
		if(bWasActive)
		{
			ActiveSaveId = SaveId;
			if(UVoxelModule* Voxel = UVoxelModule::GetPtr())
			{
				Voxel->SetActiveSaveSource(SaveId, ExistingManifest.CurrentGeneration, Storage.Get());
			}
		}
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Delete save slot failed.")));
	}

	if(bWasPersistedLast)
	{
		Storage->ClearLastActiveSave();
	}
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::RenameSaveSlot(FGuid SaveId, const FString& NewName)
{
	if (bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	FSaveManifest Manifest;
	if (!Storage || NewName.IsEmpty() || !Storage->ReadManifest(SaveId, Manifest))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save slot or name.")));
	}
	Manifest.DisplayName = NewName;
	Manifest.UpdatedAt = FDateTime::UtcNow();
	return Storage->WriteManifestAtomic(SaveId, Manifest)
	           ? FSaveOperationResult::Success()
	           : FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Rename save slot failed.")));
}

FSaveOperationResult USaveGameModule::DuplicateSaveSlot(FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary)
{
	if (bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	FSaveManifest Manifest;
	if (!Storage || NewName.IsEmpty() || !Storage->ReadManifest(SourceSaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	    !Storage->IsGenerationComplete(SourceSaveId, Manifest.CurrentGeneration, Manifest))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid source save slot.")));
	}
	const FGuid TargetSaveId = FGuid::NewGuid();
	if (!Storage->CreateWorldDirectory(TargetSaveId) ||
	    !Storage->CopyCurrentGeneration(SourceSaveId, TargetSaveId, Manifest.CurrentGeneration, Manifest.CurrentGeneration))
	{
		Storage->DeleteWorldDirectory(TargetSaveId);
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Duplicate generation failed.")));
	}
	const FString SourcePreview = FPaths::Combine(Storage->GetWorldDir(SourceSaveId), Manifest.PreviewFile);
	const FString TargetPreview = FPaths::Combine(Storage->GetWorldDir(TargetSaveId), Manifest.PreviewFile);
	if (IFileManager::Get().FileExists(*SourcePreview))
	{
		IFileManager::Get().Copy(*TargetPreview, *SourcePreview, true, true);
	}
	Manifest.SaveId = TargetSaveId;
	Manifest.DisplayName = NewName;
	Manifest.CreatedAt = FDateTime::UtcNow();
	Manifest.UpdatedAt = Manifest.CreatedAt;
	if (!Storage->WriteManifestAtomic(TargetSaveId, Manifest))
	{
		Storage->DeleteWorldDirectory(TargetSaveId);
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Duplicate manifest failed.")));
	}
	OutSummary = Manifest.ToSummary(Storage->GetWorldDir(TargetSaveId));
	return FSaveOperationResult::Success();
}

FSaveSlotSummary USaveGameModule::GetSaveSlotSummary(FGuid SaveId) const
{
	FSaveManifest Manifest;
	if (!Storage || !Storage->ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	    !Storage->IsGenerationComplete(SaveId, Manifest.CurrentGeneration, Manifest))
	{
		return FSaveSlotSummary();
	}
	return Manifest.ToSummary(Storage->GetWorldDir(SaveId));
}

TArray<FSaveSlotSummary> USaveGameModule::GetSaveSlotSummaries() const
{
	TArray<FSaveSlotSummary> Result;
	TArray<FSaveManifest> Manifests;
	if (Storage && Storage->EnumerateManifests(Manifests))
	{
		for (const FSaveManifest& Manifest : Manifests)
		{
			if (Manifest.CurrentGeneration > 0 && Storage->IsGenerationComplete(Manifest.SaveId, Manifest.CurrentGeneration, Manifest))
			{
				Result.Add(Manifest.ToSummary(Storage->GetWorldDir(Manifest.SaveId)));
			}
		}
		Result.Sort(
		    [](const FSaveSlotSummary& A, const FSaveSlotSummary& B)
		    {
			    return A.UpdatedAt > B.UpdatedAt;
		    });
	}
	return Result;
}

FSaveOperationResult USaveGameModule::SaveProfile(FName ProfileName)
{
	if (bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	USettingModule* Setting = AMainModule::GetModuleByClass<USettingModule>();
	if (!Setting)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("SettingModule missing.")));
	}
	Setting->OnBeforeSaveData();
	TArray<uint8> Bytes;
	if (!Setting->BuildSaveFile(Bytes) || !Storage->WriteBinaryAtomic(Storage->GetProfileModuleFilePath(ProfileName, Setting->GetModuleName()), Bytes))
	{
		Setting->OnAfterSaveData(false);
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Profile save failed.")));
	}
	Setting->OnAfterSaveData(true);
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::LoadProfile(FName ProfileName)
{
	if (bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	USettingModule* Setting = AMainModule::GetModuleByClass<USettingModule>();
	TArray<uint8> Bytes;
	FModuleSaveFileHeader Header;
	FParameter Data;
	if (!Setting || !Storage->ReadBinary(Storage->GetProfileModuleFilePath(ProfileName, Setting->GetModuleName()), Bytes))
	{
		return FSaveOperationResult::Failed(Setting ? ESaveResultCode::NotFound : ESaveResultCode::ModuleMissing,
		                                    FText::FromString(TEXT("Profile not found.")));
	}
	if (!FSaveDataSerializer::ReadModuleFile(Bytes, Header, Data) || Header.ModuleName != Setting->GetModuleName())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromString(TEXT("Profile data is corrupt.")));
	}
	if (Header.ModuleVersion != Setting->GetSaveDataVersion() && !Setting->MigrateSaveData(Header.ModuleVersion, Data))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::VersionMismatch, FText::FromString(TEXT("Profile version mismatch.")));
	}
	Setting->OnBeforeLoadData();
	Setting->LoadSaveData(Data, EPhase::Primary);
	Setting->LoadSaveData(Data, EPhase::Final);
	Setting->OnAfterLoadData(true);
	return FSaveOperationResult::Success();
}

TArray<UModuleBase*> USaveGameModule::GetSaveModules(ESaveScope Scope) const
{
	TArray<UModuleBase*> Result;
	for (TActorIterator<AMainModule> MainModule(GetWorld()); MainModule; ++MainModule)
	{
		for (UModuleBase* Module : MainModule->GetModules())
		{
			if (Module && Module->GetSaveScope() == Scope && Module->IsSaveEnabled())
			{
				Result.Add(Module);
			}
		}
	}
	Result.Sort(
	    [](const UModuleBase& A, const UModuleBase& B)
	    {
		    return A.GetModuleIndex() < B.GetModuleIndex();
	    });
	return Result;
}

void USaveGameModule::OnGameExited(UObject* InSender, const FEventGameExited& InEvent)
{
	const FSaveOperationResult SaveResult = SaveCurrentSlot();
	if(!SaveResult)
	{
		UE_LOG(LogTemp, Error, TEXT("Saving before exit failed: %s"), *SaveResult.Message.ToString());
	}
	SaveProfile();
}
