// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModuleTypes.h"
#include "Common/Base/WHActor.h"
#include "CameraPointBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UWorldPartitionStreamingSourceComponent;

UCLASS()
class WHFRAMEWORK_API ACameraPointBase : public AWHActor
{
	GENERATED_BODY()

public:
	ACameraPointBase();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

public:
	virtual void OnConstruction(const FTransform& Transform) override;
	
	//////////////////////////////////////////////////////////////////////////
	/// CameraPoint
public:
	UFUNCTION(BlueprintNativeEvent)
	void OnSwitch();

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void Switch(bool bSetAsDefault = false, bool bInstant = false);

public:
#if WITH_EDITOR
	void GetCameraView();
	
	void SetCameraView(const FCameraParams& InCameraParams);
#endif

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UCameraComponent* Camera;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* CameraBoom;
#endif

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UWorldPartitionStreamingSourceComponent* StreamingSource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCameraTrack;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "bCameraTrack == true"))
	ECameraTrackMode CameraTrackMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FCameraViewParams CameraViewParams;

public:
	UFUNCTION(BlueprintPure)
	bool IsDefault() const;

#if WITH_EDITORONLY_DATA
	UCameraComponent* GetCamera() const { return Camera; }

	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
#endif

	UFUNCTION(BlueprintPure)
	FCameraViewData GetCameraViewData();

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InCameraViewData"))
	void SetCameraViewData(const FCameraViewData& InCameraViewData);
};
