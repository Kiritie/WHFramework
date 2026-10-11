#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "InventoryPickupActor.generated.h"

class USphereComponent;
class UBillboardComponent;
class UInventoryWorldSubsystem;
class UInteractionComponent;

UCLASS()
class WHFRAMEWORK_API AInventoryPickupActor : public AActor, public IInteractionAgentInterface
{
    GENERATED_BODY()

public:
    AInventoryPickupActor();
    virtual UInteractionComponent* GetInteractionComponent() const override;
    virtual EInteractAgentType GetInteractAgentType() const override;
    virtual bool IsInteractable(IInteractionAgentInterface* InAgent) override;
    void SetPickupAction(FName InAction);
    FName GetPickupAction() const;
    virtual void EndPlay(const EEndPlayReason::Type InReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    bool BindInventory(UInventoryWorldSubsystem& InSystem, const FGuid& InInventoryId);
    void InitializePresentation(const FGuid& InInventoryId, const FPrimaryAssetId& InItemId, int32 InCount);

    UFUNCTION(BlueprintPure)
    FGuid GetInventoryId() const;

    UFUNCTION(BlueprintPure)
    int32 GetPublicCount() const;

    UFUNCTION(BlueprintPure)
    FPrimaryAssetId GetPublicItemId() const;

protected:
    virtual void OnEnterInteract(IInteractionAgentInterface* InAgent) override;
    virtual void OnLeaveInteract(IInteractionAgentInterface* InAgent) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UInteractionComponent> Interaction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UBillboardComponent> Presentation;

private:
    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    FName PickupAction;

    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    FGuid InventoryId;

    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    FPrimaryAssetId PublicItemId;

    UPROPERTY(ReplicatedUsing=OnRep_Presentation)
    int32 PublicCount = 0;

    TWeakObjectPtr<UInventoryWorldSubsystem> System;
    FDelegateHandle CommitHandle;
    FDelegateHandle DeleteHandle;
    void HandleCommit(const FInventoryChangeSet& InChanges);
    void HandleDeleted(const FGuid& InInventoryId);
    bool RefreshFromSnapshot();

    UFUNCTION()
    void OnRep_Presentation();
};
