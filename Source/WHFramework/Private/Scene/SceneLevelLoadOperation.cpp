#include "Scene/SceneStreamingTypes.h"
#include "Scene/SceneModule.h"

#include "Common/CommonTypes.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LatentActionManager.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Scene/Event_AsyncLoadLevelFinished.h"
#include "Event/Events/Scene/Event_AsyncUnloadLevelFinished.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Scene/Widget/WidgetLoadingLevelPanel.h"
#include "Widget/WidgetModule.h"
#include "UObject/UObjectGlobals.h"

void USceneModule::AsyncLoadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
    if(!IsBusinessAdmissionOpen() || bLevelOperationsClosing || !::IsValid(GetWorld()) || InLevelPath.IsNone())
    {
        InOnLoadFinished.ExecuteIfBound(InLevelPath, (!IsBusinessAdmissionOpen() || bLevelOperationsClosing) ? ESceneLevelLoadResult::Cancelled : ESceneLevelLoadResult::Failed);
        return;
    }
    FAsyncLoadLevelTask Task;
    Task.OperationId = NextLevelOperationId = NextLevelOperationId == MAX_int32 ? 1 : NextLevelOperationId + 1;
    Task.OperationWorld = GetWorld();
    Task.State = EFAsyncLoadLevelState::Loading;
    Task.LevelPath = InLevelPath;
    Task.OnLoadFinished = InOnLoadFinished;
    Task.FinishDelayTime = FMath::Max(0.f, InFinishDelayTime);
    Task.bCreateLoadingWidget = bCreateLoadingWidget;
    AsyncLoadLevelQueue.Add(MoveTemp(Task));
}

void USceneModule::AsyncLoadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
    AsyncLoadLevel(FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString())), InOnLoadFinished, InFinishDelayTime, bCreateLoadingWidget);
}

void USceneModule::AsyncUnloadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
    if(!IsBusinessAdmissionOpen() || bLevelOperationsClosing || !::IsValid(GetWorld()) || InLevelPath.IsNone())
    {
        InOnUnloadFinished.ExecuteIfBound(InLevelPath, (!IsBusinessAdmissionOpen() || bLevelOperationsClosing) ? ESceneLevelLoadResult::Cancelled : ESceneLevelLoadResult::Failed);
        return;
    }
    FAsyncLoadLevelTask Task;
    Task.OperationId = NextLevelOperationId = NextLevelOperationId == MAX_int32 ? 1 : NextLevelOperationId + 1;
    Task.OperationWorld = GetWorld();
    Task.State = EFAsyncLoadLevelState::Unloading;
    Task.LevelPath = InLevelPath;
    Task.OnLoadFinished = InOnUnloadFinished;
    Task.FinishDelayTime = FMath::Max(0.f, InFinishDelayTime);
    Task.bCreateLoadingWidget = bCreateLoadingWidget;
    AsyncLoadLevelQueue.Add(MoveTemp(Task));
}

void USceneModule::AsyncUnloadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
    AsyncUnloadLevel(FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString())), InOnUnloadFinished, InFinishDelayTime, bCreateLoadingWidget);
}

float USceneModule::GetAsyncLoadLevelProgress(const FName InLevelPath) const
{
    const FAsyncLoadLevelTask* Task = AsyncLoadLevelQueue.FindByPredicate([InLevelPath](const FAsyncLoadLevelTask& Item) { return Item.LevelPath == InLevelPath && Item.State == EFAsyncLoadLevelState::Loading; });
    if(!Task) return 1.f;
    if(!Task->bLoading) return 0.f;
    if(bWaitingLevelFinish && Task == &AsyncLoadLevelQueue[0]) return 1.f;
    const float Percentage = GetAsyncLoadPercentage(InLevelPath);
    return Percentage < 0.f ? 0.f : FMath::Clamp(Percentage / 100.f, 0.f, 1.f);
}

float USceneModule::GetAsyncLoadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr) const
{
    return GetAsyncLoadLevelProgress(FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString())));
}

float USceneModule::GetAsyncUnloadLevelProgress(const FName InLevelPath) const
{
    const FAsyncLoadLevelTask* Task = AsyncLoadLevelQueue.FindByPredicate([InLevelPath](const FAsyncLoadLevelTask& Item) { return Item.LevelPath == InLevelPath && Item.State == EFAsyncLoadLevelState::Unloading; });
    return Task && !(bWaitingLevelFinish && Task == &AsyncLoadLevelQueue[0]) ? 0.f : 1.f;
}

float USceneModule::GetAsyncUnloadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr) const
{
    return GetAsyncUnloadLevelProgress(FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString())));
}

void USceneModule::StartLevelOperation(FAsyncLoadLevelTask& Task)
{
    if (!IsBusinessAdmissionOpen()) return;
    Task.bLoading = true;
    const int32 OperationId = Task.OperationId;
    const FName LevelPath = Task.LevelPath;
    const EFAsyncLoadLevelState State = Task.State;
    UWorld* World = Task.OperationWorld.Get();
    if(!World || World != GetWorld() || !UGameplayStatics::GetStreamingLevel(World, Task.LevelPath))
    {
        CompleteLevelOperation(OperationId, ESceneLevelLoadResult::Failed);
        return;
    }
    RefreshLevelOperationWidgets(OperationId);
    if(!IsBusinessAdmissionOpen() || bLevelOperationsClosing || AsyncLoadLevelQueue.IsEmpty() || AsyncLoadLevelQueue[0].OperationId != OperationId) return;
    if(State == EFAsyncLoadLevelState::Unloading)
    {
        BeginStreamLevel(OperationId);
        return;
    }
    const TWeakObjectPtr<USceneModule> WeakOwner(this);
    const TWeakObjectPtr<UWorld> OriginalWorld(World);
    const FString PackagePath = FPaths::GetBaseFilename(LevelPath.ToString(), false);
    LoadPackageAsync(PackagePath, FLoadPackageAsyncDelegate::CreateLambda([WeakOwner, OriginalWorld, OperationId](const FName, UPackage*, EAsyncLoadingResult::Type Result)
    {
        if(USceneModule* Owner = WeakOwner.Get())
        {
            Owner->HandlePackageLoaded(OperationId, OriginalWorld, Result == EAsyncLoadingResult::Succeeded);
        }
    }), 0, PKG_ContainsMap);
}

void USceneModule::RefreshLevelOperationWidgets(int32 OperationId)
{
    if (!IsBusinessAdmissionOpen() || bLevelOperationsClosing || AsyncLoadLevelQueue.IsEmpty() || AsyncLoadLevelQueue[0].OperationId != OperationId) return;
    const FAsyncLoadLevelTask& Task = AsyncLoadLevelQueue[0];
    UWorld* World = Task.OperationWorld.Get();
    if (!Task.bCreateLoadingWidget || !World || World != GetWorld()) return;
    UWidgetModule* Widgets = UWidgetModule::GetPtr(World);
    UGameInstance* Instance = World->GetGameInstance();
    if (!Widgets || !Instance) return;
    const FName LevelPath = Task.LevelPath;
    const bool bUnloading = Task.State == EFAsyncLoadLevelState::Unloading;
    const TArray<ULocalPlayer*> Players = Instance->GetLocalPlayers();
    for (ULocalPlayer* LocalPlayer : Players)
    {
        APlayerController* Player = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
        if (!Player) continue;
        const UWidgetLoadingLevelPanel* Existing = Widgets->GetUserWidget<UWidgetLoadingLevelPanel>(Player);
        if (Existing && Existing->IsWidgetOpened()) continue;
        const TWeakObjectPtr<APlayerController> PlayerKey(Player);
        const bool bAlreadyOwned = AsyncLoadLevelQueue[0].LoadingWidgetPlayers.Contains(PlayerKey);
        AsyncLoadLevelQueue[0].LoadingWidgetPlayers.AddUnique(PlayerKey);
        const bool bOpened = Widgets->OpenUserWidget<UWidgetLoadingLevelPanel>(Player, FLoadingLevelWidgetOpenParameter(LevelPath, bUnloading));
        if (!IsBusinessAdmissionOpen() || bLevelOperationsClosing || AsyncLoadLevelQueue.IsEmpty() || AsyncLoadLevelQueue[0].OperationId != OperationId ||
            AsyncLoadLevelQueue[0].OperationWorld.Get() != World) return;
        if (!bOpened && !bAlreadyOwned) AsyncLoadLevelQueue[0].LoadingWidgetPlayers.Remove(PlayerKey);
    }
}

void USceneModule::HandlePackageLoaded(int32 OperationId, TWeakObjectPtr<UWorld> OperationWorld, bool bSucceeded)
{
    if(!IsBusinessAdmissionOpen() || bLevelOperationsClosing || AsyncLoadLevelQueue.IsEmpty() || AsyncLoadLevelQueue[0].OperationId != OperationId || AsyncLoadLevelQueue[0].OperationWorld != OperationWorld) return;
    if(!bSucceeded || !OperationWorld.IsValid() || OperationWorld.Get() != GetWorld())
    {
        CompleteLevelOperation(OperationId, ESceneLevelLoadResult::Failed);
        return;
    }
    BeginStreamLevel(OperationId);
}

void USceneModule::BeginStreamLevel(int32 OperationId)
{
    if (!IsBusinessAdmissionOpen()) return;
    if(AsyncLoadLevelQueue.IsEmpty() || AsyncLoadLevelQueue[0].OperationId != OperationId) return;
    const FAsyncLoadLevelTask& Task = AsyncLoadLevelQueue[0];
    UWorld* World = Task.OperationWorld.Get();
    if(!World || World != GetWorld())
    {
        CompleteLevelOperation(OperationId, ESceneLevelLoadResult::Cancelled);
        return;
    }
    FLatentActionInfo LatentInfo;
    LatentInfo.UUID = OperationId;
    LatentInfo.Linkage = OperationId;
    LatentInfo.CallbackTarget = this;
    LatentInfo.ExecutionFunction = GET_FUNCTION_NAME_CHECKED(USceneModule, OnStreamLevelFinished);
    if(Task.State == EFAsyncLoadLevelState::Loading) UGameplayStatics::LoadStreamLevel(World, Task.LevelPath, true, false, LatentInfo);
    else UGameplayStatics::UnloadStreamLevel(World, Task.LevelPath, LatentInfo, false);
}

void USceneModule::OnStreamLevelFinished(int32 OperationId)
{
    if(!IsBusinessAdmissionOpen() || bLevelOperationsClosing || AsyncLoadLevelQueue.IsEmpty() || AsyncLoadLevelQueue[0].OperationId != OperationId) return;
    const FAsyncLoadLevelTask& Task = AsyncLoadLevelQueue[0];
    UWorld* World = Task.OperationWorld.Get();
    const ULevelStreaming* Level = World && World == GetWorld() ? UGameplayStatics::GetStreamingLevel(World, Task.LevelPath) : nullptr;
    const bool bSucceeded = Level && (Task.State == EFAsyncLoadLevelState::Loading ? Level->IsLevelLoaded() && Level->IsLevelVisible() : !Level->IsLevelLoaded());
    if(!bSucceeded)
    {
        CompleteLevelOperation(OperationId, ESceneLevelLoadResult::Failed);
        return;
    }
    if(Task.FinishDelayTime > 0.f)
    {
        LevelFinishRemaining = Task.FinishDelayTime;
        bWaitingLevelFinish = true;
    }
    else CompleteLevelOperation(OperationId, ESceneLevelLoadResult::Succeeded);
}

void USceneModule::CompleteLevelOperation(int32 OperationId, ESceneLevelLoadResult Result)
{
    if(AsyncLoadLevelQueue.IsEmpty() || AsyncLoadLevelQueue[0].OperationId != OperationId) return;
    if (!IsBusinessAdmissionOpen() && Result == ESceneLevelLoadResult::Succeeded) Result = ESceneLevelLoadResult::Cancelled;
    FAsyncLoadLevelTask Task = MoveTemp(AsyncLoadLevelQueue[0]);
    AsyncLoadLevelQueue.RemoveAt(0);
    bWaitingLevelFinish = false;
    UWorld* World = Task.OperationWorld.Get();
    if(World && World == GetWorld())
    {
        if(UWidgetModule* Widgets = UWidgetModule::GetPtr(World))
        {
            for (const TWeakObjectPtr<APlayerController>& Player : Task.LoadingWidgetPlayers)
                if (Player.IsValid()) Widgets->CloseUserWidget<UWidgetLoadingLevelPanel>(Player.Get());
        }
        if (Result == ESceneLevelLoadResult::Succeeded && Task.State == EFAsyncLoadLevelState::Loading) ApplyAllOwnerPlayerMappings();
        Task.OnLoadFinished.ExecuteIfBound(Task.LevelPath, Result);
        if(!bLevelOperationsClosing && Result == ESceneLevelLoadResult::Succeeded)
        {
            if(Task.State == EFAsyncLoadLevelState::Loading) UEventModuleStatics::BroadcastEvent<FEventAsyncLoadLevelFinished>(this, { Task.LevelPath }, EEventNetType::Multicast);
            else UEventModuleStatics::BroadcastEvent<FEventAsyncUnloadLevelFinished>(this, { Task.LevelPath }, EEventNetType::Multicast);
        }
    }
    else Task.OnLoadFinished.ExecuteIfBound(Task.LevelPath, Result == ESceneLevelLoadResult::Succeeded ? ESceneLevelLoadResult::Cancelled : Result);
}

void USceneModule::CancelLevelOperations()
{
    bLevelOperationsClosing = true;
    if(UWorld* World = GetWorld()) World->GetLatentActionManager().RemoveActionsForObject(this);
    while(!AsyncLoadLevelQueue.IsEmpty()) CompleteLevelOperation(AsyncLoadLevelQueue[0].OperationId, ESceneLevelLoadResult::Cancelled);
}
