// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CameraModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "CameraModule.generated.h"

UCLASS()
class WHFRAMEWORK_API ACameraModule : public AModuleBase
{
	GENERATED_BODY()

public:
	// ParamSets default values for this actor's properties
	ACameraModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Camera
protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, Category = "Camera")
	TArray<TSubclassOf<class ACameraPawnBase>> CameraClasses;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, Category = "Camera")
	TArray<class ACameraPawnBase*> Cameras;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Replicated, Category = "Camera")
	class ACameraPawnBase* CurrentCamera;

public:
	UFUNCTION(BlueprintCallable)
	void SwitchCamera(class ACameraPawnBase* InCamera);
	
	UFUNCTION(BlueprintCallable)
	void AddCameraToList(class ACameraPawnBase* InCamera);

	UFUNCTION(BlueprintCallable)
	void RemoveCameraFromList(class ACameraPawnBase* InCamera);

	UFUNCTION(BlueprintCallable)
	void RemoveCameraFromListByName(const FName InCameraName);

	UFUNCTION(BlueprintPure)
	class ACameraPawnBase* GetCameraByName(const FName InCameraName) const;

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
