// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Parameter/ParameterModuleTypes.h"
#include "WorldWidgetComponent.generated.h"

class UWorldWidgetBase;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WHFRAMEWORK_API UWorldWidgetComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	UWorldWidgetComponent();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UWorldWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAutoCreate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOrientCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bBindToSelf;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FParameter> WidgetParams;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<FName, USceneComponent*> WidgetPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UWorldWidgetBase* Widget;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	UFUNCTION(BlueprintCallable)
	void CreateWidget();

	UFUNCTION(BlueprintCallable)
	void DestroyWidget();

public:
	UWorldWidgetBase* GetWidget() const { return Widget; }

	bool IsBindWidgetToSelf() const { return bBindToSelf; }

	UFUNCTION(BlueprintPure)
	USceneComponent* GetWidgetPoint(FName InPointName) const;
};
