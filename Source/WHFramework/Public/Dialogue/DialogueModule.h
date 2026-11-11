#pragma once

#include "Common/LifecycleTypes.h"
#include "Main/MainModuleAccess.h"
#include "Main/Base/ModuleBase.h"
#include "Dialogue/DialogueModuleTypes.h"
#include "DialogueModule.generated.h"

class UDialogueSession;
class UDialogueAsset;
class APlayerController;

UCLASS()
class WHFRAMEWORK_API UDialogueModule : public UModuleBase
{
    GENERATED_BODY()
    GENERATED_MODULE(UDialogueModule)
public:
    UDialogueModule();
    virtual ~UDialogueModule();
    virtual void OnInitialize() override;
    virtual void OnControlRefresh(float RealDeltaSeconds) override;
    virtual void OnTermination(EPhase InPhase) override;

    UFUNCTION(BlueprintCallable) bool StartDialogue(APlayerController* InPlayer, UDialogueAsset* InDialogue, AActor* InNPC);
    UFUNCTION(BlueprintCallable) bool SelectDialogueNode(APlayerController* InPlayer, int32 InNodeID);
    UFUNCTION(BlueprintCallable) bool ContinueDialogue(APlayerController* InPlayer);
    UFUNCTION(BlueprintCallable) void EndDialogue(APlayerController* InPlayer);
    UFUNCTION(BlueprintPure) bool IsDialogueActive(APlayerController* InPlayer) const;
    UFUNCTION(BlueprintPure) UDialogueAsset* GetCurrentDialogue(APlayerController* InPlayer) const;
    UFUNCTION(BlueprintPure) FDialogueNode GetCurrentNode(APlayerController* InPlayer) const;
    UFUNCTION(BlueprintPure) TArray<FDialogueNode> GetAvailableNodes(APlayerController* InPlayer) const;
    UFUNCTION(BlueprintPure) TArray<FDialogueNode> GetAvailableChoices(APlayerController* InPlayer) const;
    UFUNCTION(BlueprintPure) AActor* GetNPCActor(APlayerController* InPlayer) const;
    bool SetPersistentPlayerId(APlayerController* InPlayer, FGuid InPlayerId);
    bool BindNetworkBridge(APlayerController* InPlayer, UObject* InOwner, FDialogueNetworkBegin InBegin,
        FDialogueNetworkSelect InSelect, FDialogueNetworkContinue InContinue, FDialogueNetworkEnd InEnd);
    bool IsNetworkBridgeOwner(APlayerController* InPlayer, const UObject* InOwner) const;
    void UnbindNetworkBridge(APlayerController* InPlayer, UObject* InOwner);
    void ClearNetworkPresentation(APlayerController* InPlayer, UObject* InOwner);
    bool ApplyNetworkPresentation(APlayerController* InPlayer, UObject* InOwner, AActor* InNPC,
        const FDialogueNetworkNode& InCurrent, const TArray<FDialogueNetworkNode>& InAvailable);
    void GetNetworkNodeDecoration(APlayerController* InPlayer, int32 InNodeId, bool& bOutConditions, bool& bOutEvents, bool& bOutLinks) const;
    UPROPERTY(BlueprintAssignable) FOnPlayerDialogueChanged OnDialogueChanged;
    UPROPERTY(BlueprintAssignable) FOnPlayerDialogueChanged OnDialogueEnded;
    void ReleaseSession(APlayerController* InPlayer);

protected:
    virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;
    virtual void UnloadData(EPhase InPhase) override;
    virtual FParameter ToData() override;
private:
    friend class UDialogueSession;
    UDialogueSession* FindSession(APlayerController* InPlayer) const;
    UDialogueSession* GetOrCreateSession(APlayerController* InPlayer);
    bool BindPlayerIdentity(UDialogueSession& InSession, FGuid InPlayerId);
    void NotifyChanged(UDialogueSession& InSession, bool bEnded);
    UPROPERTY(Transient)
    TArray<TObjectPtr<UDialogueSession>> Sessions;
    UPROPERTY(Transient)
    FDialogueModuleSaveData SavedDialogue;
    bool bShuttingDown = false;
};
