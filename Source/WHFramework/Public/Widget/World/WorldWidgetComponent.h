// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Parameter/ParameterModuleTypes.h"
#include "WorldWidgetComponent.generated.h"

class UWorldWidgetBase;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WHFRAMEWORK_API UWorldWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:	
	UWorldWidgetComponent();

protected:
#if WITH_EDITOR
	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bRefreshEditorOnly;
#endif

	UPROPERTY(EditAnywhere, Category = "UserInterface")
	TSubclassOf<UWorldWidgetBase> WorldWidgetClass;
	
	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bAutoCreate;

	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bOrientCamera;

	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bBindToSelf;

	UPROPERTY(EditAnywhere, Category = "UserInterface")
	TArray<FParameter> WidgetParams;

	UPROPERTY(Transient, DuplicateTransient)
	TMap<FName, USceneComponent*> WidgetPoints;

	UPROPERTY(Transient, DuplicateTransient)
	UWorldWidgetBase* WorldWidget;

private:	
	FRotator InitRotation;

public:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void SetWidget(UUserWidget* InWidget) override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void RefreshParams();
#endif

public:
	UFUNCTION(BlueprintCallable)
	void CreateWorldWidget();

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidget();

	UFUNCTION(BlueprintCallable)
	void SetWorldWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintCallable)
	void SetWorldWidgetClass(TSubclassOf<UUserWidget> InWidgetClass, bool bCreate = false);

public:
	UFUNCTION(BlueprintCallable)
	bool IsOrientCamera() const { return bOrientCamera; }

	UFUNCTION(BlueprintCallable)
	void SetOrientCamera(bool bInOrientCamera) { this->bOrientCamera = bInOrientCamera; }

	UFUNCTION(BlueprintPure)
	bool IsBindWidgetToSelf() const { return bBindToSelf; }
	
	UFUNCTION(BlueprintPure)
	UWorldWidgetBase* GetWorldWidget() const { return WorldWidget; }

	UFUNCTION(BlueprintPure)
	USceneComponent* GetWidgetPoint(FName InPointName) const;
};
