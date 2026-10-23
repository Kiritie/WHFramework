// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameInstance.h"
#include "Engine/Engine.h"
#include "SaveGame/SaveGameModule.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

UWHGameInstance::UWHGameInstance()
{

}

void UWHGameInstance::Init()
{
	Super::Init();
	FString RequestedNamespace;
	FParse::Value(FCommandLine::Get(), TEXT("LocalDataId="), RequestedNamespace);
	auto Context = MakeShared<FSaveGameLocalContext, ESPMode::ThreadSafe>();
	if (Context->Acquire(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")), RequestedNamespace, LocalSaveContextError))
	{
		LocalSaveContext = MoveTemp(Context);
		UE_LOG(LogTemp, Display, TEXT("Local save instance acquired: namespace=%s root=%s"),
			*LocalSaveContext->GetNamespace(), *LocalSaveContext->GetStorageRoot());
	}
	else UE_LOG(LogTemp, Error, TEXT("Local save instance rejected: %s"), *LocalSaveContextError);
	if (GEngine)
	{
		SaveLoadTravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::OnSaveLoadTravelFailure);
	}
	
	FString Str;
	if(FFileHelper::LoadFileToString(Str, TEXT("E:/Wuhao/Unreal/Projects/Vigma3D/AAA.txt")))
	{
		const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Str);
		TSharedPtr<FJsonObject> JsonObject;
		if(FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			int32 A = 0;
		}
	}
}

void UWHGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(SaveLoadTravelFailureHandle);
	}
	PendingSaveLoad = FSaveGameLoadSnapshot();
	PendingLoadSource.Reset();
	Super::Shutdown();
	LocalSaveContext.Reset();
}

bool UWHGameInstance::SetPendingSaveLoad(FSaveGameLoadSnapshot InSnapshot, USaveGameModule* InSource)
{
	if (!InSnapshot.CanApply(InSnapshot.Context.LoadPhase) || InSnapshot.AppliedPhases != EPhase::None || PendingSaveLoad.IsValid())
	{
		return false;
	}
	PendingSaveLoad = MoveTemp(InSnapshot);
	PendingLoadSource = InSource;
	return true;
}

void UWHGameInstance::OnSaveLoadTravelFailure(UWorld* InWorld, ETravelFailure::Type InType, const FString& InError)
{
	USaveGameModule* Source = PendingLoadSource.Get();
	if (!PendingSaveLoad.IsValid() || !Source || Source->GetWorld() != InWorld)
	{
		return;
	}
	const FGuid OperationId = PendingSaveLoad.Context.OperationId;
	Source->CancelLoadOperation(OperationId);
	CancelPendingSaveLoad(OperationId);
	UE_LOG(LogTemp, Error, TEXT("存档切图失败：操作=%s，原因=%s"), *OperationId.ToString(), *InError);
}
