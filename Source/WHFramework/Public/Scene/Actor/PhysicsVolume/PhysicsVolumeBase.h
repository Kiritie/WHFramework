// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "PhysicsVolumeTypes.h"
#include "GameFramework/PhysicsVolume.h"
#include "Common/Base/WHActor.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "PhysicsVolumeBase.generated.h"

UCLASS()
class WHFRAMEWORK_API APhysicsVolumeBase : public APhysicsVolume, public IObjectPoolInterface, public IWHActorInterface
{
	GENERATED_BODY()
	
public:	
	APhysicsVolumeBase();

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
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	//////////////////////////////////////////////////////////////////////////
    /// Physics Volume
protected:
	UPROPERTY(VisibleAnywhere, Category = "PhysicsVolume")
	FName VolumeName;
public:
	UFUNCTION(BlueprintPure)
	FName GetVolumeName() const { return VolumeName; }

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaSeconds) override;

	virtual void Initialize(const FPhysicsVolumeData& InPhysicsVolumeData);
};
