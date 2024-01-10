// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Common/Base/WHActor.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "CameraActorBase.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class WHFRAMEWORK_API ACameraActorBase : public AWHActor
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ACameraActorBase();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return true; }

	//////////////////////////////////////////////////////////////////////////
	/// CameraActor
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

public:
#if WITH_EDITOR
	virtual bool IsUserManaged() const override { return false; }

	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UCameraComponent* Camera;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* CameraBoom;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraStats")
	FName CameraName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraStats")
	float CameraOrthoFactor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraStats")
	ECameraCollisionMode CameraCollisionMode;

public:
	UFUNCTION(BlueprintPure)
	FName GetCameraName() const { return CameraName; }

	UFUNCTION(BlueprintPure)
	UCameraComponent* GetCamera() const { return Camera; }

	UFUNCTION(BlueprintPure)
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure)
	float GetCameraOrthoFactor() const { return CameraOrthoFactor; }

	UFUNCTION(BlueprintPure)
	ECameraCollisionMode GetCameraCollisionMode() const { return CameraCollisionMode; }

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraLocation(FVector InLocation);
};
