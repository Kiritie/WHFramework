#pragma once

#include "CoreMinimal.h"
#include "Dialogue/Base/DialogueAsset.h"
#include "Dialogue/DialogueModuleTypes.h"
#include "DialogueSession.generated.h"

class UInteractionComponent;
class UDialogueModule;
class APlayerController;

UCLASS()
class WHFRAMEWORK_API UDialogueSession : public UObject
{
    GENERATED_BODY()
    friend class UDialogueModule;
public:
    virtual UWorld* GetWorld() const override;
    void Initialize(UDialogueModule& InOwner, APlayerController& InPlayer);
    void Shutdown();
    APlayerController* GetOwnerPlayer() const { return OwnerPlayer.Get(); }
    FGuid GetPersistentPlayerId() const { return PersistentPlayerId; }
    bool SetPersistentPlayerId(FGuid InId);
    FDialogueResumeData CaptureResume() const;
    void RestoreResume(const FDialogueResumeData& InData);

    UFUNCTION(BlueprintCallable) bool StartDialogue(UDialogueAsset* InDialogue, APlayerController* InPlayer, AActor* InNPC);
    UFUNCTION(BlueprintCallable) bool SelectDialogueNode(int32 InNodeID);
    UFUNCTION(BlueprintCallable) void EndDialogue();
    UFUNCTION(BlueprintPure) FDialogueNode GetCurrentNode() const;
    UFUNCTION(BlueprintPure) TArray<FDialogueNode> GetAvailableNodes() const;
    UFUNCTION(BlueprintPure) TArray<FDialogueNode> GetAvailableChoices() const;
    UFUNCTION(BlueprintCallable) bool ContinueDialogue();
    UFUNCTION(BlueprintPure) bool IsDialogueActive() const { return CurrentDialogue != nullptr; }
    UFUNCTION(BlueprintPure) UDialogueAsset* GetCurrentDialogue() const { return CurrentDialogue; }
    UFUNCTION(BlueprintPure) AActor* GetNPCActor() const { return NPCActor; }
    UFUNCTION(BlueprintPure) APlayerController* GetConsideringPlayer() const { return ConsideringPlayer; }

    bool BindNetworkBridge(UObject* InOwner, FDialogueNetworkBegin InBegin, FDialogueNetworkSelect InSelect,
        FDialogueNetworkContinue InContinue, FDialogueNetworkEnd InEnd);
    bool IsNetworkBridgeOwner(const UObject* InOwner) const;
    void UnbindNetworkBridge(UObject* InOwner);
    void ClearNetworkPresentation();
    bool ApplyNetworkPresentation(UObject* InOwner, APlayerController* InPlayer, AActor* InNPC,
        const FDialogueNetworkNode& InCurrent, const TArray<FDialogueNetworkNode>& InAvailable);
    void GetNetworkNodeDecoration(int32 InNodeId, bool& bOutConditions, bool& bOutEvents, bool& bOutLinks) const;

private:
    bool HasNetworkBridge() const;
    bool IsNodeAvailable(const FDialogueNode& Node) const;
    bool EnterNode(int32 InNodeID, bool bRunEvents);
    UFUNCTION() void OnParticipantDestroyed(AActor* InActor);
    TWeakObjectPtr<UDialogueModule> OwnerModule;
    TWeakObjectPtr<APlayerController> OwnerPlayer;
    FGuid PersistentPlayerId;
    UPROPERTY(Transient) TObjectPtr<UDialogueAsset> CurrentDialogue;
    UPROPERTY(Transient) TObjectPtr<UDialogueAsset> SourceDialogue;
    UPROPERTY(Transient) TObjectPtr<APlayerController> ConsideringPlayer;
    UPROPERTY(Transient) TObjectPtr<AActor> NPCActor;
    int32 CurrentNodeID = INDEX_NONE;
    UPROPERTY(Transient) FDialogueResumeData SavedDialogue;
    bool bSelectingNode = false;
    TWeakObjectPtr<UInteractionComponent> Interaction;
    TWeakObjectPtr<AActor> BoundPawn;
    bool bNetworkPresentation = false;
    TWeakObjectPtr<UObject> NetworkBridgeOwner;
    FDialogueNetworkBegin NetworkBegin;
    FDialogueNetworkSelect NetworkSelect;
    FDialogueNetworkContinue NetworkContinue;
    FDialogueNetworkEnd NetworkEnd;
    TMap<int32, FDialogueNetworkNode> NetworkNodes;
};
