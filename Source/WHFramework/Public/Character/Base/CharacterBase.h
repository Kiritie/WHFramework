// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CharacterInterface.h"
#include "AI/Base/AIAgentInterface.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "GameFramework/Character.h"
#include "Audio/AudioModuleTypes.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Voxel/Agent/VoxelAgentInterface.h"

#include "CharacterBase.generated.h"

class UAIPerceptionStimuliSourceComponent;
/**
 * 
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API ACharacterBase : public ACharacter, public ICharacterInterface, public IWHPlayerInterface, public IAIAgentInterface, public IVoxelAgentInterface, public IObjectPoolInterface, public ISceneActorInterface, public IPrimaryEntityInterface, public IWHActorInterface
{
	GENERATED_BODY()
	
public:
	ACharacterBase();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return true; }

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaSeconds) override;
	
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void SpawnDefaultController() override;

	//////////////////////////////////////////////////////////////////////////
	/// Name
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FName Name;
public:
	virtual FName GetNameC() const override { return Name; }

	virtual void SetNameC(FName InName) override { Name = InName; }

	//////////////////////////////////////////////////////////////////////////
	/// Player
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* CameraBoom;

protected:
	virtual void Turn_Implementation(float InValue) override;

	virtual void MoveForward_Implementation(float InValue) override;

	virtual void MoveRight_Implementation(float InValue) override;

	virtual void MoveUp_Implementation(float InValue) override;

public:
	virtual UCameraComponent* GetCameraComp_Implementation() override { return FollowCamera; }

	virtual USpringArmComponent* GetCameraBoom_Implementation() override { return CameraBoom; }

	//////////////////////////////////////////////////////////////////////////
	/// Actor
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor")
	FGuid ActorID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor")
	bool bVisible;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Actor")
	TScriptInterface<ISceneContainerInterface> Container;
	
public:
	virtual FGuid GetActorID_Implementation() const override { return ActorID; }

	virtual void SetActorID_Implementation(FGuid InActorID) override { ActorID = InActorID; }

	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }

	virtual bool IsVisible_Implementation() const override { return bVisible; }

	virtual void SetActorVisible_Implementation(bool bNewVisible) override;

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
	/// Voxel
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FPrimaryAssetId GenerateVoxelID;

public:
	virtual FVector GetWorldLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }

public:
	virtual bool GenerateVoxel(const FVoxelHitResult& InVoxelHitResult) override;

	virtual bool DestroyVoxel(const FVoxelHitResult& InVoxelHitResult) override;

	//////////////////////////////////////////////////////////////////////////
	/// Sound
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FSingleSoundHandle SoundHandle;
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
	UFUNCTION(BlueprintPure)
	virtual FPrimaryAssetId GetAssetID() const override { return AssetID; }
	
	template<class T>
	T& GetCharacterData() const
	{
		return static_cast<T&>(GetCharacterData());
	}
	
	UPrimaryAssetBase& GetCharacterData() const;

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
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
