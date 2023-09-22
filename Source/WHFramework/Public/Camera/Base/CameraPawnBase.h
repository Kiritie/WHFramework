// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraModuleTypes.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "CameraPawnBase.generated.h"

UCLASS(hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod))
class WHFRAMEWORK_API ACameraPawnBase : public APawn, public IWHPlayerInterface, public IVoxelAgentInterface, public IWHActorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ACameraPawnBase();

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
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UCameraComponent* Camera;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UPawnMovementComponent* MovementComponent;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraStats")
	FName CameraName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraStats")
	ECameraCollisionMode CameraCollisionMode;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaSeconds) override;

public:
	UFUNCTION(BlueprintPure)
	FName GetCameraName() const { return CameraName; }

	UCameraComponent* GetCameraComp_Implementation() override { return Camera; }

	USpringArmComponent* GetCameraBoom_Implementation() override { return CameraBoom; }

	UFUNCTION(BlueprintPure)
	ECameraCollisionMode GetCameraCollisionMode() const { return CameraCollisionMode; }

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode);

	//////////////////////////////////////////////////////////////////////////
	/// Voxel
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CameraStats")
	FPrimaryAssetId GenerateVoxelID;

public:
	virtual FVector GetVoxelAgentLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }

public:
	virtual bool OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult) override;

	virtual bool OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult) override;
};
