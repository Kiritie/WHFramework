// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Event/Handle/InstancedEventHandleBase.h"

#include "EventHandle_SetCameraView.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_SetCameraView : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_SetCameraView();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;

public:
#if WITH_EDITOR
	virtual void GetCameraView();
	
	virtual void SetCameraView(const FCameraParams& InCameraParams);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere)
	bool bRefreshData;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FCameraViewData CameraViewData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCacheData;
};
