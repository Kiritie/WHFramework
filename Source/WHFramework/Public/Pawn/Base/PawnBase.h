// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PawnInterface.h"
#include "AI/Base/AIAgentInterface.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "GameFramework/Pawn.h"
#include "Common/Base/WHActor.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Voxel/Agent/VoxelAgentInterface.h"

#include "PawnBase.generated.h"

class UBoxComponent;
class UAIPerceptionStimuliSourceComponent;
/**
 * 
 */
UCLASS(meta=(ShortTooltip="A Pawn is an actor that can be 'possessed' and receive input from a controller."))
class WHFRAMEWORK_API APawnBase : public APawn, public IPawnInterface, public IWHPlayerInterface, public IAIAgentInterface, public IVoxelAgentInterface, public IObjectPoolInterface, public ISceneActorInterface, public IPrimaryEntityInterface, public IWHActorInterface
{
	GENERATED_BODY()
	
public:
	APawnBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return true; }

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaSeconds) override;
	
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void SpawnDefaultController() override;

	//////////////////////////////////////////////////////////////////////////
	/// Components
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;
public:
	virtual UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	//////////////////////////////////////////////////////////////////////////
	/// Name
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PawnStats")
	FName Name;
public:
	virtual FName GetNameP() const override { return Name; }

	virtual void SetNameP(FName InName) override { Name = InName; }

	//////////////////////////////////////////////////////////////////////////
	/// Player
protected:
	virtual void Turn_Implementation(float InValue) override;

	virtual void MoveForward_Implementation(float InValue) override;

	virtual void MoveRight_Implementation(float InValue) override;

	virtual void MoveUp_Implementation(float InValue) override;

	//////////////////////////////////////////////////////////////////////////
	/// Camera
public:
	UFUNCTION(BlueprintNativeEvent)
	FVector GetCameraTraceOffset() const override;

	//////////////////////////////////////////////////////////////////////////
	/// Actor
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SceneActor")
	FGuid ActorID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SceneActor")
	bool bVisible;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SceneActor")
	TScriptInterface<ISceneContainerInterface> Container;
	
public:
	virtual FGuid GetActorID_Implementation() const override { return ActorID; }

	virtual void SetActorID_Implementation(const FString& InID) override { ActorID = FGuid(InID); }

	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }

	virtual bool IsVisible_Implementation() const override { return bVisible; }

	virtual void SetActorVisible_Implementation(bool bInVisible) override;

	//////////////////////////////////////////////////////////////////////////
	/// Controller
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Component")
	AController* DefaultController;
public:
	virtual AController* GetDefaultController() const override { return DefaultController; }

	//////////////////////////////////////////////////////////////////////////
	/// Voxel
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PawnStats")
	FPrimaryAssetId GenerateVoxelID;

public:
	virtual FVector GetVoxelAgentLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }

public:
	virtual bool OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult) override;

	virtual bool OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult) override;

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
	virtual FPrimaryAssetId GetAssetID_Implementation() const override { return AssetID; }
	
	virtual void SetAssetID_Implementation(const FPrimaryAssetId& InID) override { AssetID = InID; }
	
	template<class T>
	T& GetPawnData() const
	{
		return static_cast<T&>(GetPawnData());
	}
	
	UPrimaryAssetBase& GetPawnData() const;

	//////////////////////////////////////////////////////////////////////////
	/// AI
protected: 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAIPerceptionStimuliSourceComponent* StimuliSource;

public:
	UFUNCTION(BlueprintPure)
	virtual UBehaviorTree* GetBehaviorTreeAsset() const override;
	
	UAIPerceptionStimuliSourceComponent* GetStimuliSource() const { return StimuliSource; }

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	UFUNCTION(BlueprintPure)
	virtual FVector GetMoveVelocity(bool bIgnoreZ = true) const;

	UFUNCTION(BlueprintPure)
	virtual FVector GetMoveDirection(bool bIgnoreZ = true) const;

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
