// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CharacterInterface.h"
#include "AI/Base/AIAgentInterface.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "GameFramework/Character.h"
#include "Audio/AudioModuleTypes.h"
#include "Common/Base/WHActor.h"
#include "Common/Looking/LookingAgentInterface.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Voxel/Agent/VoxelAgentInterface.h"

#include "CharacterBase.generated.h"

class ULookingComponent;
class UCharacterDataBase;
class UAIPerceptionStimuliSourceComponent;
/**
 * 
 */
UCLASS(meta=(ShortTooltip="A character is a type of Pawn that includes the ability to walk around."))
class WHFRAMEWORK_API ACharacterBase : public ACharacter, public ICharacterInterface, public IWHPlayerInterface, public IAIAgentInterface, public IVoxelAgentInterface, public IObjectPoolInterface, public ISaveDataAgentInterface, public IPrimaryEntityInterface, public IWHActorInterface, public ILookingAgentInterface
{
	GENERATED_BODY()
	
public:
	ACharacterBase(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WHActor")
	bool bInitialized;
	
protected:
	virtual bool IsInitialized_Implementation() const override { return bInitialized; }
	
	virtual bool IsUseDefaultLifecycle_Implementation() const override { return true; }

	//////////////////////////////////////////////////////////////////////////
	/// SaveData
public:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaSeconds) override;
	
	virtual void SpawnDefaultController() override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Character
public:
	UFUNCTION(BlueprintNativeEvent)
	void OnSwitch();

	UFUNCTION(BlueprintNativeEvent)
	void OnUnSwitch();

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void Switch();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void UnSwitch();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool IsCurrent() const;

	//////////////////////////////////////////////////////////////////////////
	/// Name
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterStats")
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

	virtual void JumpN_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Camera
public:
	virtual FVector GetCameraOffset_Implementation() const override;

	virtual float GetCameraDistance_Implementation() const override;

	virtual float GetCameraMinPitch_Implementation() const override;

	virtual float GetCameraMaxPitch_Implementation() const override;

	virtual ECameraTrackMode GetCameraTrackMode_Implementation() const override;

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
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Components")
	UCharacterAnimBase* Anim;
	
public:
	virtual UCharacterAnimBase* GetAnim() const override { return Anim; }

	//////////////////////////////////////////////////////////////////////////
	/// Controller
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Components")
	AController* DefaultController;
	
public:
	template<class T>
	T* GetDefaultController() const
	{
		return Cast<T>(DefaultController);
	}
	
	virtual AController* GetDefaultController() const override { return DefaultController; }

	virtual bool IsUseControllerRotation() const override;

	virtual void SetUseControllerRotation(bool bValue) override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Voxel
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FPrimaryAssetId GenerateVoxelID;

public:
	virtual FVector GetVoxelAgentLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }

	//////////////////////////////////////////////////////////////////////////
	/// Sound
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FSingleSoundHandle SoundHandle;
	
public:
	virtual void PlaySound(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false) override;
	
	virtual void StopSound(bool bMulticast = false) override;

	//////////////////////////////////////////////////////////////////////////
	/// Montage
public:
	virtual void PlayMontage(UAnimMontage* InMontage, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlayMontage(UAnimMontage* InMontage);
	virtual void PlayMontageByName(const FName InMontageName, bool bMulticast = false) override;

	virtual void StopMontage(UAnimMontage* InMontage, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopMontage(UAnimMontage* InMontage);
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
	/// Looking
protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	ULookingComponent* Looking;

public:
	UFUNCTION()
	virtual void OnTargetLookAtOn(AActor* InTargetActor);
	
	UFUNCTION()
	virtual void OnTargetLookAtOff(AActor* InTargetActor);
	
public:
	virtual bool IsLookAtAble_Implementation(AActor* InLookerActor) const override;
	
	UFUNCTION(BlueprintPure)
	virtual bool CanLookAtTarget();

	UFUNCTION(BlueprintPure)
	virtual ULookingComponent* GetLooking() const { return Looking; }

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
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UAIPerceptionStimuliSourceComponent* StimuliSource;

public:
	UFUNCTION(BlueprintPure)
	virtual UBehaviorTree* GetBehaviorTreeAsset() const override;
	
	virtual AAIControllerBase* GetAIController() const override;

	UAIPerceptionStimuliSourceComponent* GetStimuliSource() const { return StimuliSource; }

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	UFUNCTION(BlueprintPure)
	virtual FVector GetMoveVelocity(bool bIgnoreZ = false) const;

	UFUNCTION(BlueprintPure)
	virtual FVector GetMoveDirection(bool bIgnoreZ = false) const;

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
