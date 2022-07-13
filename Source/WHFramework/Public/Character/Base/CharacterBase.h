// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterInterface.h"
#include "AI/Base/AIAgentInterface.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "GameFramework/Character.h"
#include "Audio/AudioModuleTypes.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Voxel/Agent/VoxelAgentInterface.h"

#include "CharacterBase.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API ACharacterBase : public ACharacter, public ICharacterInterface, public IAIAgentInterface, public IVoxelAgentInterface, public IObjectPoolInterface, public ISceneActorInterface, public IPrimaryEntityInterface
{
	GENERATED_BODY()
	
public:
	ACharacterBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

	virtual void SpawnDefaultController() override;
		
	//////////////////////////////////////////////////////////////////////////
	/// Name
protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CharacterStats")
	FName Name;
public:
	virtual FName GetNameC() const override { return Name; }

	virtual void SetNameC(FName InName) override { Name = InName; }

	virtual FName GetObjectName_Implementation() const override { return Name; }

	virtual void SetObjectName_Implementation(FName InName) override { Name = InName; }

	//////////////////////////////////////////////////////////////////////////
	/// Anim
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Component")
	UCharacterAnim* Anim;
public:
	virtual UCharacterAnim* GetAnim() const override { return Anim; }

	//////////////////////////////////////////////////////////////////////////
	/// Controller
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Component")
	AController* DefaultController;
public:
	virtual AController* GetDefaultController() const override { return DefaultController; }

	//////////////////////////////////////////////////////////////////////////
	/// Chunk
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	AVoxelChunk* OwnerChunk;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FVoxelItem GeneratingVoxelItem;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FVoxelItem SelectedVoxelItem;

public:
	UFUNCTION(BlueprintCallable)
	virtual bool GenerateVoxel(FVoxelItem& InVoxelItem) override;

	virtual bool GenerateVoxel(FVoxelItem& InVoxelItem, const FVoxelHitResult& InVoxelHitResult) override;

	UFUNCTION(BlueprintCallable)
	virtual bool DestroyVoxel(FVoxelItem& InVoxelItem) override;

	virtual bool DestroyVoxel(const FVoxelHitResult& InVoxelHitResult) override;
	
	virtual FVector GetWorldLocation() const override { return GetActorLocation(); }
	
	UFUNCTION(BlueprintPure)
	virtual AVoxelChunk* GetOwnerChunk() const override { return OwnerChunk; }

	UFUNCTION(BlueprintCallable)
	virtual void SetOwnerChunk(AVoxelChunk* InOwnerChunk) override { OwnerChunk = InOwnerChunk; }

	virtual FVoxelItem& GetGeneratingVoxelItem() override { return GeneratingVoxelItem; }

	virtual void SetGeneratingVoxelItem(FVoxelItem InGeneratingVoxelItem) override { GeneratingVoxelItem = InGeneratingVoxelItem; }

	virtual FVoxelItem& GetSelectedVoxelItem() override { return SelectedVoxelItem; }

	virtual void SetSelectedVoxelItem(FVoxelItem InSelectedVoxelItem) override { SelectedVoxelItem = InSelectedVoxelItem; }

	//////////////////////////////////////////////////////////////////////////
	/// Sound
public:
	virtual void PlaySound(class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false) override;
	
	virtual void StopSound(bool bMulticast = false) override;

	//////////////////////////////////////////////////////////////////////////
	/// Montage
public:
	virtual void PlayMontage(class UAnimMontage* InMontage, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlayMontage(class UAnimMontage* InMontage);
	virtual void PlayMontageByName(const FName InMontageName, bool bMulticast = false) override;

	virtual void StopMontage(class UAnimMontage* InMontage, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopMontage(class UAnimMontage* InMontage);
	virtual void StopMontageByName(const FName InMontageName, bool bMulticast = false) override;

	//////////////////////////////////////////////////////////////////////////
	/// Transform
public:
	virtual void TransformTowards(FTransform InTransform, float InDuration = 1.f, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiTransformTowards(FTransform InTransform, float InDuration = 1.f);

	//////////////////////////////////////////////////////////////////////////
	/// Rotation
public:
	virtual void RotationTowards(FRotator InRotation, float InDuration = 1.f, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiRotationTowards(FRotator InRotation, float InDuration = 1.f);

	//////////////////////////////////////////////////////////////////////////
	/// AI Move
public:
	virtual void AIMoveTo(FVector InLocation, float InStopDistance = 10.f, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiAIMoveTo(FVector InLocation, float InStopDistance = 10.f);

	virtual void StopAIMove(bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopAIMove();

	//////////////////////////////////////////////////////////////////////////
	/// Primary Asset
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FPrimaryAssetId AssetID;

public:
	virtual FPrimaryAssetId GetAssetID() const override { return AssetID; }

	//////////////////////////////////////////////////////////////////////////
	/// AI
public:
	virtual UBehaviorTree* GetBehaviorTreeAsset() const override;

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
