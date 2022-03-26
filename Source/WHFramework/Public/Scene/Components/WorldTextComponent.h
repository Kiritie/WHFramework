// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Components/WidgetComponent.h"
#include "Widget/World/WorldWidgetComponent.h"
#include "WorldTextComponent.generated.h"

/**
 * �����ı����
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class DREAMWORLD_API UWorldTextComponent : public UWorldWidgetComponent
{
	GENERATED_BODY()

public:
	UWorldTextComponent();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TMap<EWorldTextStyle, TSubclassOf<class UWidgetWorldText>> WorldTextMap;

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	void InitContent(FString InContent, EWorldTextType InContentType, EWorldTextStyle InContentStyle);
	
protected:
	void RefreshVisibility();
};
