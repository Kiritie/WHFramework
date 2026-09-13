// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Common/CommonModuleTypes.h"
#include "Components/WidgetComponent.h"
#include "CommonUserWidget.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"
#include "Slate/Runtime/Interfaces/PanelWidgetInterface.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"

#include "WorldWidgetBase.generated.h"

class UCanvasPanelSlot;
/**
 * 
 */
UCLASS(BlueprintType, meta = (DisableNativeTick))
class WHFRAMEWORK_API UWorldWidgetBase : public UCommonUserWidget, public IPanelWidgetInterface, public IObjectPoolInterface
{
	friend class UWidgetModule;
	friend class UWorldWidgetComponent;
	
	GENERATED_BODY()

public:
	UWorldWidgetBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

public:
	virtual bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

	virtual void OnTick_Implementation(float DeltaSeconds) override;

public:
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnCreate")
	void K2_OnCreate(const FParameter& InParam);
	UFUNCTION()
	virtual void OnCreate(FWorldWidgetMapping InMapping, const FParameter& InParam);
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize(const FParameter& InParam);
	UFUNCTION()
	virtual void OnInitialize(const FParameter& InParam);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnReset")
	void K2_OnReset(bool bForce = false);
	UFUNCTION()
	virtual void OnReset(bool bForce = false) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh();
	UFUNCTION()
	virtual void OnRefresh() override;
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnDestroy")
	void K2_OnDestroy(EObjectDespawnMode InMode);
	UFUNCTION()
	virtual void OnDestroy(EObjectDespawnMode InMode) override;

public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParam"))
	virtual void Init(const FParameter& InParam = FParameter(), bool bForce = false);
	
	UFUNCTION(BlueprintCallable)
	virtual void Reset(bool bForce = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void Refresh() override;

	UFUNCTION(BlueprintCallable)
	virtual void Destroy(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy) override;

public:
	template<class T>
	T* CreateSubWidget(const FParameter& InParam = FParameter(), TSubclassOf<UUserWidget> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSubWidget(InClass, InParam));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParam"), DisplayName = "CreateSubWidget")
	UUserWidget* K2_CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam = FParameter());

	virtual ISubWidgetInterface* CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam = FParameter()) override;

	UFUNCTION(BlueprintCallable, DisplayName = "DestroySubWidget")
	bool K2_DestroySubWidget(UUserWidget* InWidget, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	virtual bool DestroySubWidget(ISubWidgetInterface* InWidget, EObjectDespawnMode InMode) override;

	UFUNCTION(BlueprintCallable)
	virtual void DestroyAllSubWidget(EObjectDespawnMode InMode) override;

protected:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RefreshLocation(UWidget* InWidget, FWorldWidgetMapping InMapping);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RefreshVisibility();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RefreshLocationAndVisibility();

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void BindWidgetPoint(UWidget* InWidget, FWorldWidgetMapping InMapping);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void UnBindWidgetPoint(UWidget* InWidget);
	
	UFUNCTION(BlueprintPure, BlueprintNativeEvent)
	bool GetWidgetMapping(UWidget* InWidget, FWorldWidgetMapping& OutMapping);
	
	UFUNCTION(BlueprintPure, BlueprintNativeEvent)
	bool IsWidgetVisible(bool bRefresh = false);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget", meta = (Categories = "Widget.World"))
	FGameplayTag DefaultWidgetTag;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Widget")
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetRefreshType WidgetRefreshType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "WidgetRefreshType == EWidgetRefreshType::Timer"))
	float WidgetRefreshTime;
	
	UPROPERTY(Transient)
	FParameter InitializeParameter;

	UPROPERTY(Transient)
	bool bInitialized;

	UPROPERTY(Transient)
	UObject* OwnerObject;

	UPROPERTY(Transient)
	bool bWidgetInEditor;

	UPROPERTY(Transient)
	UWorldWidgetComponent* WidgetComponent;

	UPROPERTY(Transient)
	TMap<UWidget*, FWorldWidgetMapping> BindWidgetMap;

	UPROPERTY(Transient)
	TArray<FSubWidgetRuntimeEntry> SubWidgetEntries;

public:
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnDestroyed"))
	FK2_OnWidgetDestroyed K2_OnDestroyed;

	FOnWidgetDestroyed OnDestroyed;

private:
	FTimerHandle RefreshTimerHandle;
	TSharedPtr<SWidget> WorldWidget;
	
public:
	UFUNCTION(BlueprintPure)
	FGameplayTag GetDefaultWidgetTag() const { return DefaultWidgetTag; }

	UFUNCTION(BlueprintPure)
	FGameplayTag GetWidgetTag() const { return WidgetTag; }

	const FWorldWidgetConfig* GetWidgetConfig() const;

	UFUNCTION(BlueprintPure)
	virtual int32 GetWidgetZOrder() const override;

	UFUNCTION(BlueprintPure)
	virtual FAnchors GetWidgetAnchors() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetAutoSize() const override;

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetDrawSize() const override;

	UFUNCTION(BlueprintPure)
	virtual FMargin GetWidgetOffsets() const override;

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetAlignment() const override;

	UFUNCTION(BlueprintPure)
	EWorldWidgetVisibility GetWidgetVisibility() const;

	UFUNCTION(BlueprintPure)
	float GetWidgetShowDistance() const;

	UFUNCTION(BlueprintPure)
	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }

	UFUNCTION(BlueprintPure)
	virtual FParameter GetWidgetParams() const override { return InitializeParameter; }

	template<class T>
	T* GetOwnerObject() const
	{
		return Cast<T>(OwnerObject);
	}

	virtual UObject* GetOwnerObject() const override { return OwnerObject; }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual UObject* GetOwnerObject(TSubclassOf<UObject> InClass) const { return GetDeterminesOutputObject(OwnerObject, InClass); }

	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetInEditor() const { return bWidgetInEditor; }

	UFUNCTION(BlueprintPure)
	UWorldWidgetComponent* GetWidgetComponent() const { return WidgetComponent; }

	UFUNCTION(BlueprintPure)
	EWorldWidgetSpace GetWorldWidgetSpace() const;

	UFUNCTION(BlueprintPure)
	EWidgetSpace GetWidgetSpace() const;
	
	UFUNCTION(BlueprintPure)
	UPanelWidget* GetRootPanelWidget() const;

	UFUNCTION(BlueprintPure)
	TArray<UWidget*> GetPoolWidgets() const;

	UFUNCTION(BlueprintPure)
	virtual int32 GetSubWidgetNum() const override { return SubWidgetEntries.Num(); }

	template<class T>
	TArray<T*> GetSubWidgets()
	{
		TArray<T*> ReturnValues;
		for(auto Iter : GetSubWidgets())
		{
			ReturnValues.Add(Cast<T>(Iter));
		}
		return ReturnValues;
	}
	virtual TArray<ISubWidgetInterface*> GetSubWidgets() override;
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), DisplayName = "GetSubWidgets")
	virtual TArray<UUserWidget*> K2_GetSubWidgets(TSubclassOf<UUserWidget> InClass);

	template<class T>
	T* GetSubWidget(int32 InIndex) const
	{
		return Cast<T>(GetSubWidget(InIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidget* GetSubWidget(int32 InIndex, TSubclassOf<UUserWidget> InClass) const;

	UFUNCTION(BlueprintPure)
	int32 FindSubWidget(UUserWidget* InWidget) const;

	virtual ISubWidgetInterface* GetSubWidget(int32 InIndex) const override
	{
		if(SubWidgetEntries.IsValidIndex(InIndex))
		{
			return Cast<ISubWidgetInterface>(SubWidgetEntries[InIndex].Widget);
		}
		return nullptr;
	}

	virtual int32 FindSubWidget(ISubWidgetInterface* InWidget) const override
	{
		for(int32 Index = 0; Index < SubWidgetEntries.Num(); ++Index)
		{
			if(Cast<ISubWidgetInterface>(SubWidgetEntries[Index].Widget) == InWidget)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
};
