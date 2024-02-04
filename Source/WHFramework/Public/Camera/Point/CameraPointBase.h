// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModuleTypes.h"
#include "Common/Base/WHActor.h"
#include "CameraPointBase.generated.h"

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
	
	//////////////////////////////////////////////////////////////////////////
	/// CameraPoint
public:
	UFUNCTION(BlueprintNativeEvent)
	void OnSwitch();

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void Switch(bool bCachePoint = true, bool bSetAsDefault = false);

public:
#if WITH_EDITOR
	void GetCameraView();
	
	void SetCameraView(const FCameraParams& InCameraParams);
#endif

protected:
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

	UFUNCTION(BlueprintPure)
	FCameraViewData GetCameraViewData();

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InCameraViewData"))
	void SetCameraViewData(const FCameraViewData& InCameraViewData);
};
