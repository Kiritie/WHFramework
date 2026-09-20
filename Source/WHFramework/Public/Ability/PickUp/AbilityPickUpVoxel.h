#pragma once
#include "UObject/PrimaryAssetId.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "AbilityPickUpVoxel.generated.h"
class UVoxelMeshComponent;
class UVoxelSceneRegion;
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpVoxel : public AAbilityPickUpBase
{
	GENERATED_BODY()
public:
	AAbilityPickUpVoxel();
	static AAbilityPickUpVoxel* CreateReserved(UWorld* World, const FAbilityItem& Item, const FVector& Location, UVoxelSceneRegion* Region);
	void ActivateReserved();
	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& Container) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void LoadData(const FParameter& Data, EPhase Phase) override;
	virtual FParameter ToData() override;
	virtual UMeshComponent* GetMeshComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* Picker) override;
	virtual void OnBeginOverlap(
	    UPrimitiveComponent* Component, AActor* Actor, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep, const FHitResult& Hit) override;
	virtual void OnEnterInteract(IInteractionAgentInterface* Agent) override;
	UFUNCTION()
	void OnRepState();
	UPROPERTY(ReplicatedUsing = OnRepState)
	FPrimaryAssetId RepID;
	UPROPERTY(ReplicatedUsing = OnRepState)
	int32 RepCount = 0;
	UPROPERTY(ReplicatedUsing = OnRepState)
	int32 RepLevel = 0;
	UPROPERTY(ReplicatedUsing = OnRepState)
	bool bActivated = false;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVoxelMeshComponent>> DisplayMeshes;
	UPROPERTY(Transient)
	TObjectPtr<UVoxelSceneRegion> OwningRegion;

private:
	bool BuildVisual();
	bool bVisualDirty = true;
};
