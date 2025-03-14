// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/Base/WHActor.h"
#include "WorldWidgetActor.generated.h"

class UWorldWidgetComponent;

UCLASS()
class WHFRAMEWORK_API AWorldWidgetActor : public AWHActor
{
	GENERATED_BODY()

public:
	AWorldWidgetActor();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual void PostInitProperties() override;

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Destroyed() override;
	
#if WITH_EDITOR
	virtual void OnRefreshWorldWidget();
#endif

	//////////////////////////////////////////////////////////////////////////
	/// CameraPoint
public:
	UFUNCTION(BlueprintCallable)
	void CreateWorldWidget(bool bInEditor = false);

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidget(bool bRecovery = false, bool bInEditor = false);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	TArray<FParameter> GetWidgetParams() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWorldWidgetComponent* WorldWidget;
	
	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bAutoCreate;
	
	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bShowInEditor;

public:
	UFUNCTION(BlueprintPure)
	UWorldWidgetComponent* GetWorldWidget() const { return WorldWidget; }
};
