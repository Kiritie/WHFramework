#include "SaveGame/SaveGameModule.h"

#include "Event/EventModuleStatics.h"
#include "Gameplay/WHGameInstance.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveDataSerializer.h"
#include "SaveGame/SaveGameStorage.h"
#include "Setting/SettingModule.h"
#include "Voxel/VoxelModule.h"

IMPLEMENTATION_MODULE(USaveGameModule)

USaveGameModule::USaveGameModule()
{
	ModuleName = FName("SaveGameModule");
	ModuleDisplayName = FText::FromString(TEXT("Save Game Module"));
	bModuleRequired = true;
	SaveScope = ESaveScope::None;
	UserIndex = 0;
	bSaveOperationRunning = false;
}

USaveGameModule::~USaveGameModule() = default;

void USaveGameModule::OnInitialize()
{
	Super::OnInitialize();
	Storage = MakeUnique<FSaveGameStorage>(UserIndex);
	Storage->EnsureRoot();
	Storage->CleanupAllTempGenerations();
	if(UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		PendingLoadContext = GameInstance->ConsumePendingSaveLoad();
	}
	UEventModuleStatics::SubscribeEvent<FEventGameExited>(this, &ThisClass::OnGameExited);
}

void USaveGameModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	if(PHASEC(InPhase, EPhase::Final) && PendingLoadContext.IsValid())
	{
		const FPendingSaveLoadContext Context = PendingLoadContext;
		PendingLoadContext = FPendingSaveLoadContext();
		RestoreSlotGeneration(Context);
	}
}

void USaveGameModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
	if(PHASEC(InPhase, EPhase::Final))
	{
		Storage.Reset();
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
	UserIndex = InUserIndex;
	Storage = MakeUnique<FSaveGameStorage>(UserIndex);
	Storage->EnsureRoot();
}

FSaveOperationResult USaveGameModule::CreateSaveSlot(const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary)
{
	if(!Storage || bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save system is busy.")));
	}
	TGuardValue<bool> Guard(bSaveOperationRunning, true);
	FSaveManifest Manifest;
	Manifest.SaveId = FGuid::NewGuid();
	Manifest.DisplayName = Params.DisplayName;
	Manifest.Description = Params.Description;
	Manifest.CreatedAt = FDateTime::UtcNow();
	Manifest.UpdatedAt = Manifest.CreatedAt;
	Manifest.CurrentMap = Params.InitialMap;
	if(!Storage->CreateWorldDirectory(Manifest.SaveId) || !Storage->WriteManifestAtomic(Manifest.SaveId, Manifest))
	{
		Storage->DeleteWorldDirectory(Manifest.SaveId);
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Create save slot failed.")));
	}
	ActiveSaveId = Manifest.SaveId;
	if(Params.bCaptureCurrentWorld)
	{
		const FSaveOperationResult Result = SaveSlotInternal(Manifest.SaveId);
		if(!Result)
		{
			return Result;
		}
		Storage->ReadManifest(Manifest.SaveId, Manifest);
	}
	OutSummary = Manifest.ToSummary(Storage->GetWorldDir(Manifest.SaveId));
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::SaveSlot(FGuid SaveId)
{
	if(!SaveId.IsValid())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save id.")));
	}
	if(!Storage || bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save system is busy.")));
	}
	TGuardValue<bool> Guard(bSaveOperationRunning, true);
	return SaveSlotInternal(SaveId);
}

FSaveOperationResult USaveGameModule::SaveActiveSlot()
{
	return ActiveSaveId.IsValid() ? SaveSlot(ActiveSaveId) : FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("No active save.")));
}

FSaveOperationResult USaveGameModule::SaveSlotInternal(FGuid SaveId)
{
	FSaveManifest Manifest;
	if(!Storage->ReadManifest(SaveId, Manifest))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("Save slot not found.")));
	}
	Storage->CleanupUncommittedGenerations(SaveId, Manifest.CurrentGeneration);
	const int32 NewGeneration = Manifest.CurrentGeneration + 1;
	if(!Storage->PrepareTempGeneration(SaveId, Manifest.CurrentGeneration, NewGeneration))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Prepare generation failed.")));
	}
	TArray<UModuleBase*> Modules = GetSaveModules(ESaveScope::World);
	for(UModuleBase* Module : Modules)
	{
		Module->OnBeforeSaveData();
	}
	TArray<UModuleBase*> Captured;
	FSaveOperationResult Result = CaptureModulesToGeneration(SaveId, NewGeneration, Captured);
	if(!Result || !Storage->CommitGeneration(SaveId, NewGeneration))
	{
		for(UModuleBase* Module : Modules)
		{
			Module->OnAfterSaveData(false);
		}
		return Result ? FSaveOperationResult::Failed(ESaveResultCode::CommitFailed, FText::FromString(TEXT("Generation commit failed."))) : Result;
	}
	Manifest.CurrentGeneration = NewGeneration;
	Manifest.UpdatedAt = FDateTime::UtcNow();
	if(UWorld* World = GetWorld())
	{
		Manifest.CurrentMap = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	}
	for(UModuleBase* Module : Modules)
	{
		Manifest.ModuleVersions.Add(Module->GetModuleName(), Module->GetSaveDataVersion());
	}
	if(!Storage->WriteManifestAtomic(SaveId, Manifest))
	{
		for(UModuleBase* Module : Modules)
		{
			Module->OnAfterSaveData(false);
		}
		return FSaveOperationResult::Failed(ESaveResultCode::CommitFailed, FText::FromString(TEXT("Manifest update failed.")));
	}
	for(UModuleBase* Module : Modules)
	{
		Module->OnAfterSaveData(true);
	}
	ActiveSaveId = SaveId;
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::CaptureModulesToGeneration(const FGuid& SaveId, int32 Generation, TArray<UModuleBase*>& OutCaptured)
{
	for(UModuleBase* Module : GetSaveModules(ESaveScope::World))
	{
		TArray<uint8> Bytes;
		if(!Module->BuildSaveFile(Bytes))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromName(Module->GetModuleName()));
		}
		if(!Storage->WriteBinary(Storage->GetTempModuleFilePath(SaveId, Generation, Module->GetModuleName()), Bytes))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromName(Module->GetModuleName()));
		}
		OutCaptured.Add(Module);
	}
	if(UVoxelModule* Voxel = AMainModule::GetModuleByClass<UVoxelModule>(false))
	{
		if(Voxel->IsSaveEnabled() && !Voxel->WritePendingRegionsToGeneration(SaveId, Generation, *Storage))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Voxel region write failed.")));
		}
	}
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::LoadSlot(FGuid SaveId)
{
	if(!SaveId.IsValid())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save id.")));
	}
	if(!Storage || bSaveOperationRunning)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save system is busy.")));
	}
	TGuardValue<bool> Guard(bSaveOperationRunning, true);
	FSaveManifest Manifest;
	if(!Storage->ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("Committed save slot not found.")));
	}
	const FName CurrentMap(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
	if(!Manifest.CurrentMap.IsNone() && Manifest.CurrentMap != CurrentMap)
	{
		PendingLoadContext.SaveId = SaveId;
		PendingLoadContext.Generation = Manifest.CurrentGeneration;
		PendingLoadContext.TargetMap = Manifest.CurrentMap;
		if(UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
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
	return RestoreSlotGeneration(Context);
}

FSaveOperationResult USaveGameModule::RestoreSlotGeneration(const FPendingSaveLoadContext& Context)
{
	FSaveOperationResult Result = LoadModulesFromGeneration(Context.SaveId, Context.Generation);
	if(Result)
	{
		ActiveSaveId = Context.SaveId;
	}
	return Result;
}

FSaveOperationResult USaveGameModule::LoadModulesFromGeneration(const FGuid& SaveId, int32 Generation)
{
	struct FLoadedModuleSaveData
	{
		UModuleBase* Module = nullptr;
		FModuleSaveFileHeader Header;
		FParameter Data;
	};
	TArray<FLoadedModuleSaveData> Loaded;
	for(UModuleBase* Module : GetSaveModules(ESaveScope::World))
	{
		TArray<uint8> Bytes;
		FLoadedModuleSaveData Item;
		Item.Module = Module;
		if(!Storage->ReadBinary(Storage->GetModuleFilePath(SaveId, Generation, Module->GetModuleName()), Bytes))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::ReadFailed, FText::FromName(Module->GetModuleName()));
		}
		if(!FSaveDataSerializer::ReadModuleFile(Bytes, Item.Header, Item.Data) || Item.Header.ModuleName != Module->GetModuleName())
		{
			return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromName(Module->GetModuleName()));
		}
		if(Item.Header.ModuleVersion != Module->GetSaveDataVersion() && !Module->MigrateSaveData(Item.Header.ModuleVersion, Item.Data))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::VersionMismatch, FText::FromName(Module->GetModuleName()));
		}
		Loaded.Add(MoveTemp(Item));
	}
	if(UVoxelModule* Voxel = AMainModule::GetModuleByClass<UVoxelModule>(false))
	{
		Voxel->SetActiveSaveSource(SaveId, Generation, Storage.Get());
	}
	for(FLoadedModuleSaveData& Item : Loaded)
	{
		Item.Module->OnBeforeLoadData();
	}
	for(FLoadedModuleSaveData& Item : Loaded)
	{
		Item.Module->LoadSaveData(Item.Data, EPhase::Primary);
	}
	for(FLoadedModuleSaveData& Item : Loaded)
	{
		Item.Module->LoadSaveData(Item.Data, EPhase::Final);
	}
	for(FLoadedModuleSaveData& Item : Loaded)
	{
		Item.Module->OnAfterLoadData(true);
	}
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::DeleteSaveSlot(FGuid SaveId)
{
	if(!Storage || !SaveId.IsValid())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save id.")));
	}
	if(SaveId == ActiveSaveId)
	{
		ActiveSaveId.Invalidate();
	}
	return Storage->DeleteWorldDirectory(SaveId) ? FSaveOperationResult::Success() : FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Delete save slot failed.")));
}

FSaveOperationResult USaveGameModule::RenameSaveSlot(FGuid SaveId, const FString& NewName)
{
	FSaveManifest Manifest;
	if(!Storage || NewName.IsEmpty() || !Storage->ReadManifest(SaveId, Manifest))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save slot or name.")));
	}
	Manifest.DisplayName = NewName;
	Manifest.UpdatedAt = FDateTime::UtcNow();
	return Storage->WriteManifestAtomic(SaveId, Manifest) ? FSaveOperationResult::Success() : FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Rename save slot failed.")));
}

FSaveOperationResult USaveGameModule::DuplicateSaveSlot(FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary)
{
	FSaveManifest Manifest;
	if(!Storage || NewName.IsEmpty() || !Storage->ReadManifest(SourceSaveId, Manifest) || Manifest.CurrentGeneration <= 0)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid source save slot.")));
	}
	const FGuid TargetSaveId = FGuid::NewGuid();
	if(!Storage->CreateWorldDirectory(TargetSaveId) || !Storage->CopyCurrentGeneration(SourceSaveId, TargetSaveId, Manifest.CurrentGeneration, Manifest.CurrentGeneration))
	{
		Storage->DeleteWorldDirectory(TargetSaveId);
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Duplicate generation failed.")));
	}
	const FString SourcePreview = FPaths::Combine(Storage->GetWorldDir(SourceSaveId), Manifest.PreviewFile);
	const FString TargetPreview = FPaths::Combine(Storage->GetWorldDir(TargetSaveId), Manifest.PreviewFile);
	if(IFileManager::Get().FileExists(*SourcePreview))
	{
		IFileManager::Get().Copy(*TargetPreview, *SourcePreview, true, true);
	}
	Manifest.SaveId = TargetSaveId;
	Manifest.DisplayName = NewName;
	Manifest.CreatedAt = FDateTime::UtcNow();
	Manifest.UpdatedAt = Manifest.CreatedAt;
	if(!Storage->WriteManifestAtomic(TargetSaveId, Manifest))
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
	return Storage && Storage->ReadManifest(SaveId, Manifest) ? Manifest.ToSummary(Storage->GetWorldDir(SaveId)) : FSaveSlotSummary();
}

TArray<FSaveSlotSummary> USaveGameModule::GetSaveSlotSummaries() const
{
	TArray<FSaveSlotSummary> Result;
	TArray<FSaveManifest> Manifests;
	if(Storage && Storage->EnumerateManifests(Manifests))
	{
		for(const FSaveManifest& Manifest : Manifests)
		{
			Result.Add(Manifest.ToSummary(Storage->GetWorldDir(Manifest.SaveId)));
		}
		Result.Sort([](const FSaveSlotSummary& A, const FSaveSlotSummary& B) { return A.UpdatedAt > B.UpdatedAt; });
	}
	return Result;
}

FSaveOperationResult USaveGameModule::SaveProfile(FName ProfileName)
{
	USettingModule* Setting = AMainModule::GetModuleByClass<USettingModule>();
	if(!Setting)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("SettingModule missing.")));
	}
	Setting->OnBeforeSaveData();
	TArray<uint8> Bytes;
	if(!Setting->BuildSaveFile(Bytes) || !Storage->WriteBinaryAtomic(Storage->GetProfileModuleFilePath(ProfileName, Setting->GetModuleName()), Bytes))
	{
		Setting->OnAfterSaveData(false);
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Profile save failed.")));
	}
	Setting->OnAfterSaveData(true);
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::LoadProfile(FName ProfileName)
{
	USettingModule* Setting = AMainModule::GetModuleByClass<USettingModule>();
	TArray<uint8> Bytes;
	FModuleSaveFileHeader Header;
	FParameter Data;
	if(!Setting || !Storage->ReadBinary(Storage->GetProfileModuleFilePath(ProfileName, Setting->GetModuleName()), Bytes))
	{
		return FSaveOperationResult::Failed(Setting ? ESaveResultCode::NotFound : ESaveResultCode::ModuleMissing, FText::FromString(TEXT("Profile not found.")));
	}
	if(!FSaveDataSerializer::ReadModuleFile(Bytes, Header, Data) || Header.ModuleName != Setting->GetModuleName())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromString(TEXT("Profile data is corrupt.")));
	}
	if(Header.ModuleVersion != Setting->GetSaveDataVersion() && !Setting->MigrateSaveData(Header.ModuleVersion, Data))
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
	for(UModuleBase* Module : AMainModule::GetAllModule())
	{
		if(Module && Module != this && Module->IsSaveEnabled() && Module->GetSaveScope() == Scope)
		{
			Result.Add(Module);
		}
	}
	Result.Sort([](const UModuleBase& A, const UModuleBase& B) { return A.GetModuleIndex() < B.GetModuleIndex(); });
	return Result;
}

void USaveGameModule::OnGameExited(UObject* InSender, const FEventGameExited& InEvent)
{
	if(HasActiveSave())
	{
		SaveActiveSlot();
	}
	SaveProfile();
}
