// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Base/ModuleBase.h"
#include "Object/PhysicsVolume/PhysicsVolumeTypes.h"

#include "SceneModule.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API ASceneModule : public AModuleBase
{
	GENERATED_BODY()

public:
	ASceneModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
    /// Scene Object
protected:
	UPROPERTY(EditAnywhere, Category = "SceneObject")
	TMap<FName, TScriptInterface<class ISceneObject>> SceneObjects;

public:
	UFUNCTION(BlueprintPure, Category = "SceneObject")
	bool HasSceneObject(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "SceneObject")
	TScriptInterface<class ISceneObject> GetSceneObject(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "TargetPoint")
	void SetSceneObject(FName InName, TScriptInterface<class ISceneObject> InObject);

	//////////////////////////////////////////////////////////////////////////
    /// Target Point
protected:
	UPROPERTY(EditAnywhere, Category = "TargetPoint")
	TMap<FName, class ATargetPoint*> TargetPoints;

public:
	UFUNCTION(BlueprintPure, Category = "TargetPoint")
	bool HasTargetPoint(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "TargetPoint")
	ATargetPoint* GetTargetPoint(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "TargetPoint")
	void SetTargetPoint(FName InName, ATargetPoint* InPoint);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Point
protected:
	UPROPERTY(EditAnywhere, Category = "ScenePoint")
	TMap<FName, USceneComponent*> ScenePoints;

public:
	UFUNCTION(BlueprintPure, Category = "ScenePoint")
	bool HasScenePoint(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "ScenePoint")
	USceneComponent* GetScenePoint(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "ScenePoint")
	void SetScenePoint(FName InName, USceneComponent* InSceneComp);

	//////////////////////////////////////////////////////////////////////////
    /// Physics Volume
protected:
	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TMap<FName, class APhysicsVolumeBase*> PhysicsVolumes;

	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TArray<FPhysicsVolumeData> DefaultPhysicsVolumes;

public:
	UFUNCTION(BlueprintPure, Category = "PhysicsVolumes")
	bool HasPhysicsVolume(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "PhysicsVolumes")
	class APhysicsVolumeBase* GetPhysicsVolume(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void SetPhysicsVolume(FName InName, class APhysicsVolumeBase* InPhysicsVolume);
};
