// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CameraModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "CameraModule.generated.h"

class ACameraPawnBase;
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
	UPROPERTY(EditAnywhere, Category = "Camera")
	TSubclassOf<ACameraPawnBase> DefaultCameraClass;

	UPROPERTY(EditAnywhere, Category = "Camera")
	TArray<TSubclassOf<ACameraPawnBase>> CameraClasses;
	
	UPROPERTY(EditAnywhere, Category = "Camera")
	TArray<ACameraPawnBase*> Cameras;
		
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	ACameraPawnBase* CurrentCamera;

private:			
	UPROPERTY(Transient)
	TMap<FName, ACameraPawnBase*> CameraMap;

public:
	template<class T>
	T* GetCamera(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		return Cast<T>(K2_GetCamera(InClass));
	}

	UFUNCTION(BlueprintCallable, DisplayName = "GetCamera", meta = (DeterminesOutputType = "InClass"))
	ACameraPawnBase* K2_GetCamera(TSubclassOf<ACameraPawnBase> InClass);

	template<class T>
	T* GetCameraByName(const FName InCameraName)
	{
		return Cast<T>(K2_GetCameraByName(InCameraName));
	}

	UFUNCTION(BlueprintPure, DisplayName = "GetCamera")
	ACameraPawnBase* K2_GetCameraByName(const FName InCameraName) const;

	template<class T>
	void SwitchCamera(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		K2_SwitchCamera(InClass);
	}

	UFUNCTION(BlueprintCallable, DisplayName = "SwitchCamera")
	void K2_SwitchCamera(TSubclassOf<ACameraPawnBase> InClass);

	UFUNCTION(BlueprintCallable)
	void SwitchCameraByName(const FName InCameraName);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
