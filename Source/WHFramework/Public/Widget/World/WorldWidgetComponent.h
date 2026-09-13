// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Parameter/ParameterModuleTypes.h"
#include "WorldWidgetComponent.generated.h"

class UWorldWidgetBase;
struct FWorldWidgetConfig;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), hidecategories=(Physics, Collision, HLOD, PathTracing, Natigation, VirtualTexture, Tags, Cooking, MaterialParameters, TextureStreaming, Mobile, RayTracing, AssetUserData))
class WHFRAMEWORK_API UWorldWidgetComponent : public UWidgetComponent
{
	friend class AWorldWidgetActor;
	
	GENERATED_BODY()

public:	
	UWorldWidgetComponent();

public:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void SetWidget(UUserWidget* InWidget) override;

	virtual void RefreshParams();

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParam"))
	void CreateWorldWidget(const FParameter& InParam = FParameter(), bool bInEditor = false);

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidget(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, bool bInEditor = false);

	UFUNCTION(BlueprintCallable)
	void SetWorldWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintCallable)
	void SetWorldWidgetClass(TSubclassOf<UWorldWidgetBase> InClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"))
	void SetWorldWidgetTag(FGameplayTag InTag, bool bRefresh = false);

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bRefreshEditorOnly;
#endif

	UPROPERTY(EditAnywhere, Category = "UserInterface", meta = (Categories = "Widget.World"))
	FGameplayTag WorldWidgetTag;

	UPROPERTY(EditAnywhere, Category = "UserInterface")
	TSubclassOf<UWorldWidgetBase> WorldWidgetClassOverride;
	
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "EDC_AutoCreate"), Category = "UserInterface")
	bool bAutoCreate;

	UPROPERTY(EditAnywhere, Category = "UserInterface")
	bool bOrientCamera;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "Space == EWidgetSpace::Screen"), Category = "UserInterface")
	bool bBindToSelf;

	UPROPERTY(EditAnywhere, Category = "UserInterface")
	FParameter WidgetParam;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UserInterface")
	FVector WidgetScale;

	UPROPERTY(Transient, DuplicateTransient)
	TMap<FName, USceneComponent*> WidgetPoints;

	UPROPERTY(Transient, DuplicateTransient)
	UWorldWidgetBase* WorldWidget;

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

protected:
	UFUNCTION()
	bool EDC_AutoCreate() const;

private:
	const FWorldWidgetConfig* ResolveWorldWidgetConfig() const;

	TSubclassOf<UWorldWidgetBase> ResolveWorldWidgetClass() const;
};
