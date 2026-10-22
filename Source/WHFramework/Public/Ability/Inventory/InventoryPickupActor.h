#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "InventoryPickupActor.generated.h"

class USphereComponent;
class UMaterialBillboardComponent;
class UWidgetComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UInventoryWorldSubsystem;
class UInteractionComponent;

/** 任意库存物品的世界投影。库存快照是唯一物品真值，Actor 不直接增删物品。 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API AInventoryPickupActor : public AActor, public IInteractionAgentInterface
{
    GENERATED_BODY()

public:
    AInventoryPickupActor();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type InReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual UInteractionComponent* GetInteractionComponent() const override;
    virtual EInteractAgentType GetInteractAgentType() const override;
    virtual bool IsInteractable(IInteractionAgentInterface* InAgent) override;

    bool BindInventory(UInventoryWorldSubsystem& InSystem, const FGuid& InInventoryId);
    void InitializePresentation(const FGuid& InInventoryId, const FPrimaryAssetId& InItemId, int32 InCount);
    void SetPickupAction(FName InAction);
    void SetPickupCollisionProfile(FName InProfile);
    void SetPickupPublished(bool bInPublished);
    FName GetPickupAction() const;

    UFUNCTION(BlueprintPure)
    FGuid GetInventoryId() const;

    UFUNCTION(BlueprintPure)
    int32 GetPublicCount() const;

    UFUNCTION(BlueprintPure)
    FPrimaryAssetId GetPublicItemId() const;

protected:
    virtual void OnEnterInteract(IInteractionAgentInterface* InAgent) override;
    virtual void OnLeaveInteract(IInteractionAgentInterface* InAgent) override;

    // 子类只可扩展表现，不可在此发物品或修改持久库存。
    UFUNCTION(BlueprintImplementableEvent)
    void OnPickupPresentationChanged(FPrimaryAssetId InItemId, int32 InCount);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UInteractionComponent> Interaction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USceneComponent> VisualRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UWidgetComponent> IconPresentation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UMaterialBillboardComponent> MaterialIconPresentation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMeshComponent> StaticPresentation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USkeletalMeshComponent> SkeletalPresentation;

private:
    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    bool bPublished = false;

    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    FName PickupAction;

    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    FName PickupCollisionProfile = TEXT("OverlapOnlyPawn");

    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    FGuid InventoryId;

    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    FPrimaryAssetId PublicItemId;

    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    int32 PublicCount = 0;

    TWeakObjectPtr<UInventoryWorldSubsystem> System;
    FDelegateHandle CommitHandle;
    FDelegateHandle DeleteHandle;
    FTimerHandle PresentationRetry;
    double PresentationDeadline = 0.0;
    void HandleCommit(const FInventoryChangeSet& InChanges);
    void HandleDeleted(const FGuid& InInventoryId);
    bool RefreshFromSnapshot();
    void RefreshVisual();
    void SetHighlighted(bool bInHighlighted);

    UFUNCTION()
    void OnRep_Presentation();
};
