// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CharacterInterface.h"
#include "AI/Base/AIAgentInterface.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "GameFramework/Character.h"
#include "Audio/AudioModuleTypes.h"
#include "Common/Base/WHActor.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Voxel/Agent/VoxelAgentInterface.h"

#include "CharacterBase.generated.h"

class UCharacterDataBase;
class UAIPerceptionStimuliSourceComponent;
/**
 * 
 */
UCLASS(meta=(ShortTooltip="A character is a type of Pawn that includes the ability to walk around."))
class WHFRAMEWORK_API ACharacterBase : public ACharacter, public ICharacterInterface, public IWHPlayerInterface, public IAIAgentInterface, public IVoxelAgentInterface, public IObjectPoolInterface, public ISceneActorInterface, public IPrimaryEntityInterface, public IWHActorInterface
{
	GENERATED_BODY()
	
public:
	ACharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	/// Name
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FName Name;
public:
	virtual FName GetNameP() const override { return Name; }

	virtual void SetNameP(FName InName) override { Name = InName; }

	virtual FName GetNameC() const override { return Name; }

	virtual void SetNameC(FName InName) override { Name = InName; }

	//////////////////////////////////////////////////////////////////////////
	/// Player
protected:
	virtual void Turn_Implementation(float InValue) override;

	virtual void MoveForward_Implementation(float InValue) override;

	virtual void MoveRight_Implementation(float InValue) override;

	virtual void MoveUp_Implementation(float InValue) override;

	virtual void JumpN_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Camera
public:
	virtual FVector GetCameraOffset_Implementation() const override;

	virtual float GetCameraDistance_Implementation() const override;

	virtual ECameraSmoothMode GetCameraSmoothMode_Implementation() const override;

	virtual ECameraControlMode GetCameraControlMode_Implementation() const override;

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
	/// Anim
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Component")
	UCharacterAnimBase* Anim;
public:
	virtual UCharacterAnimBase* GetAnim() const override { return Anim; }

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
	virtual FVector GetVoxelAgentLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }

public:
	virtual bool OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult) override;

	virtual bool OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult) override;

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
	virtual FPrimaryAssetId GetAssetID_Implementation() const override { return AssetID; }
	
	virtual void SetAssetID_Implementation(const FPrimaryAssetId& InID) override { AssetID = InID; }
	
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
