// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bRefreshEditorOnly;

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
	FTransform InitTransform;

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
	void DestroyWorldWidget(bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void SetWorldWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintCallable)
	void SetWorldWidgetClass(TSubclassOf<UUserWidget> InClass, bool bRefresh = false);

public:
	UFUNCTION(BlueprintCallable)
	bool IsAutoCreate() const { return bAutoCreate; }

	UFUNCTION(BlueprintCallable)
	void SetAutoCreate(bool bInAutoCreate) { this->bAutoCreate = bInAutoCreate; }

	UFUNCTION(BlueprintCallable)
	bool IsOrientCamera() const { return bOrientCamera; }

	UFUNCTION(BlueprintCallable)
	void SetOrientCamera(bool bInOrientCamera) { this->bOrientCamera = bInOrientCamera; }

	UFUNCTION(BlueprintPure)
	bool IsBindToSelf() const { return bBindToSelf; }
	
	UFUNCTION(BlueprintCallable)
	void SetBindToSelf(bool bInBindToSelf) { this->bBindToSelf = bInBindToSelf; }
	
	UFUNCTION(BlueprintPure)
	UUserWidget* GetWorldWidget() const;

	UFUNCTION(BlueprintPure)
	USceneComponent* GetWidgetPoint(FName InPointName) const;
};
