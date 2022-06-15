// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterInterface.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "GameFramework/Character.h"
#include "Audio/AudioModuleTypes.h"
#include "Scene/Actor/SceneActorInterface.h"

#include "CharacterBase.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API ACharacterBase : public ACharacter, public ICharacterInterface, public ISceneActorInterface, public IPrimaryEntityInterface
{
	GENERATED_BODY()
	
public:
	ACharacterBase();

protected:
	virtual void BeginPlay() override;
	
public:
	virtual void Tick(float DeltaSeconds) override;

	//////////////////////////////////////////////////////////////////////////
	/// Name
protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
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
	/// Sound
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Components")
	UAudioComponent* SingleSound;
	
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_SingleSoundInfo, Category = "SoundActionInfos")
	FSingleSoundInfo SingleSoundInfo;

private:
	FTimerHandle SingleSoundStopTimerHandle;

public:
	virtual void PlaySound(class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false) override;
	
	virtual void StopSound() override;
	
protected:
	UFUNCTION()
	virtual void OnRep_SingleSoundInfo();

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
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
