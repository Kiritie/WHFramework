// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/RichTextBlockDecorator.h"
#include "Main/Base/ModuleBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widget/Screen/UserWidgetBase.h"
#include "World/WorldWidgetBase.h"
#include "Debug/DebugModuleTypes.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "WidgetModule.generated.h"

class UWorldWidgetContainer;
struct FEventSetWorldWidgetVisible;
struct FEventCloseUserWidget;
struct FEventOpenUserWidget;

UCLASS()
class WHFRAMEWORK_API UWidgetModule : public UModuleBase
{
	GENERATED_BODY()
			
	GENERATED_MODULE(UWidgetModule)

public:	
	UWidgetModule();

	~UWidgetModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	virtual FString GetModuleDebugMessage() override;

	void RegisterTickableWidget(UUserWidget* InWidget);
	void UnregisterTickableWidget(UUserWidget* InWidget);

private:
	UPROPERTY(Transient)
	TSet<UUserWidget*> TickableWidgets;

protected:
	UFUNCTION()
	void OnOpenUserWidget(UObject* InSender, const FEventOpenUserWidget& InEvent);

	UFUNCTION()
	void OnCloseUserWidget(UObject* InSender, const FEventCloseUserWidget& InEvent);

	////////////////////////////////////////////////////
	// GlobalSettings
protected:
	UPROPERTY(EditAnywhere, Category = "GlobalSettings")
	TArray<FLanguageType> LanguageTypes;

	UPROPERTY(EditAnywhere, Category = "GlobalSettings")
	int32 LanguageType;

	UPROPERTY(EditAnywhere, Category = "GlobalSettings")
	float GlobalScale;

public:
	UFUNCTION(BlueprintPure)
	TArray<FLanguageType> GetLanguageTypes() const { return LanguageTypes; }

	UFUNCTION(BlueprintCallable)
	void SetLanguageTypes(const TArray<FLanguageType>& InLanguageTypes) { LanguageTypes = InLanguageTypes; }

	UFUNCTION(BlueprintPure)
	int32 GetLanguageType() const { return LanguageType; }

	UFUNCTION(BlueprintCallable)
	void SetLanguageType(int32 InLanguageType)
	{
		LanguageType = InLanguageType;
		if(LanguageTypes.IsValidIndex(InLanguageType))
		{
			UKismetInternationalizationLibrary::SetCurrentCulture(LanguageTypes[LanguageType].LocalCulture);
		}
	}

	UFUNCTION(BlueprintPure)
	float GetGlobalScale() const { return GlobalScale; }

	UFUNCTION(BlueprintCallable)
	void SetGlobalScale(float InGlobalScale) { GlobalScale = InGlobalScale; }

	////////////////////////////////////////////////////
	// CommonWidget
protected:
	UPROPERTY(EditAnywhere, Category = "CommonWidget|RichText")
	UDataTable* CommonRichTextStyle;

	UPROPERTY(EditAnywhere, Category = "CommonWidget|RichText")
	TArray<TSubclassOf<URichTextBlockDecorator>> CommonRichTextDecorators;

public:
	UFUNCTION(BlueprintPure)
	UDataTable* GetCommonRichTextStyle() const { return CommonRichTextStyle; }

	UFUNCTION(BlueprintCallable)
	void SetCommonRichTextStyle(UDataTable* InCommonRichTextStyle) { CommonRichTextStyle = InCommonRichTextStyle; }

	UFUNCTION(BlueprintPure)
	TArray<TSubclassOf<URichTextBlockDecorator>> GetCommonRichTextDecorators() const { return CommonRichTextDecorators; }

	UFUNCTION(BlueprintCallable)
	void SetCommonRichTextDecorators(const TArray<TSubclassOf<URichTextBlockDecorator>>& InCommonRichTextDecorators) { CommonRichTextDecorators = InCommonRichTextDecorators; }

	UFUNCTION(BlueprintCallable)
	void AddCommonRichTextDecorator(const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator) { CommonRichTextDecorators.Add(InCommonRichTextDecorator); }

	UFUNCTION(BlueprintCallable)
	void RemoveCommonRichTextDecorator(const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator) { CommonRichTextDecorators.Remove(InCommonRichTextDecorator); }

	////////////////////////////////////////////////////
	// ScreenWidget
protected:
	UPROPERTY(EditAnywhere, Category = "ScreenWidget")
	TArray<FScreenWidgetConfig> UserWidgetConfigs;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, int32> UserWidgetConfigIndexMap;

	TMultiMap<TObjectPtr<UClass>, FGameplayTag> UserWidgetClassTagMap;

	TMultiMap<FGameplayTag, FGameplayTag> UserWidgetChildrenMap;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UUserWidgetBase>> UserWidgetByTag;

	UPROPERTY(Transient)
	TMap<FWidgetMountContext, TObjectPtr<UUserWidgetBase>> ActiveTemporaryWidgets;

private:
	void BuildRuntimeCaches();

	const FScreenWidgetConfig* GetUserWidgetConfig(FGameplayTag InWidgetTag) const;

	FGameplayTag ResolveWidgetTagForClass(
		TSubclassOf<UUserWidgetBase> InClass,
		bool bEnsured = true) const;

	bool EnsureParentCreated(
		const FScreenWidgetConfig& InConfig,
		UObject* InOwner);

	void ApplyWidgetConfig(
		UUserWidgetBase* InWidget,
		const FScreenWidgetConfig& InConfig) const;

	bool AttachWidgetToConfiguredParent(
		UUserWidgetBase* InWidget,
		const FScreenWidgetConfig& InConfig);

public:
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InExpectedClass", AutoCreateRefTerm = "InWidgetTag"))
	UUserWidgetBase* GetUserWidgetByTag(
		FGameplayTag InWidgetTag,
		TSubclassOf<UUserWidgetBase> InExpectedClass = nullptr) const;

	UUserWidgetBase* CreateUserWidgetByTag(
		FGameplayTag InWidgetTag,
		UObject* InOwner = nullptr,
		const FParameter* InInitParameter = nullptr,
		TSubclassOf<UUserWidgetBase> InClassOverride = nullptr);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Create User Widget By Tag", DeterminesOutputType = "InClassOverride", AutoCreateRefTerm = "InWidgetTag,InInitParameter"))
	UUserWidgetBase* K2_CreateUserWidgetByTag(
		FGameplayTag InWidgetTag,
		UObject* InOwner,
		const FParameter& InInitParameter,
		TSubclassOf<UUserWidgetBase> InClassOverride = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag,InOpenParameter"))
	bool OpenUserWidgetByTag(
		FGameplayTag InWidgetTag,
		const FParameter& InOpenParameter,
		bool bInstant = false,
		bool bForce = false,
		TSubclassOf<UUserWidgetBase> InClassOverride = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"))
	bool CloseUserWidgetByTag(
		FGameplayTag InWidgetTag,
		bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"))
	bool ToggleUserWidgetByTag(
		FGameplayTag InWidgetTag,
		bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"))
	bool DestroyUserWidgetByTag(
		FGameplayTag InWidgetTag,
		bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	bool CloseActiveTemporaryWidget(
		const FWidgetMountContext& InContext,
		bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParentWidgetTag,InParentSlotTag"))
	bool CloseTemporaryWidgetInSlot(
		FGameplayTag InParentWidgetTag,
		FGameplayTag InParentSlotTag,
		bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParentWidgetTag"))
	void CloseTemporaryWidgetsForParent(
		FGameplayTag InParentWidgetTag,
		bool bInstant = false);

	template<class T>
	bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		return ResolveWidgetTagForClass(InClass, false).IsValid();
	}

	UFUNCTION(BlueprintPure)
	bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const;

	template<class T>
	TSubclassOf<UUserWidgetBase> GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		const FScreenWidgetConfig* Config = GetUserWidgetConfig(
			ResolveWidgetTagForClass(InClass));
		return Config ? Config->WidgetClass : nullptr;
	}

	UFUNCTION(BlueprintPure)
	TSubclassOf<UUserWidgetBase> GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass) const;

	template<class T>
	bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		return GetUserWidgetByTag(ResolveWidgetTagForClass(InClass, false)) != nullptr;
	}

	UFUNCTION(BlueprintPure)
	bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass) const;

	template<class T>
	T* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass()) const
	{
		return Cast<T>(GetUserWidgetByTag(ResolveWidgetTagForClass(InClass)));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UUserWidgetBase* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass) const;

	template<class T>
	T* CreateUserWidget(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass)
		{
			return nullptr;
		}
		const FParameter* Parameter = InParams && !InParams->IsEmpty() ? &(*InParams)[0] : nullptr;
		return Cast<T>(CreateUserWidgetByTag(ResolveWidgetTagForClass(InClass), InOwner, Parameter));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false);

	template<class T>
	bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass)
		{
			return false;
		}
		const FParameter Parameter = InParams && !InParams->IsEmpty() ? (*InParams)[0] : FParameter();
		return OpenUserWidgetByTag(ResolveWidgetTagForClass(InClass), Parameter, bInstant, bForce);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false);

	template<class T>
	bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return InClass && CloseUserWidgetByTag(ResolveWidgetTagForClass(InClass), bInstant);
	}

	UFUNCTION(BlueprintCallable)
	bool CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	template<class T>
	bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return InClass && ToggleUserWidgetByTag(ResolveWidgetTagForClass(InClass), bInstant);
	}

	UFUNCTION(BlueprintCallable)
	bool ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	template<class T>
	bool DestroyUserWidget(bool bRecovery = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return InClass && DestroyUserWidgetByTag(ResolveWidgetTagForClass(InClass), bRecovery);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void CloseAllUserWidget(bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void ClearAllUserWidget(bool bRecovery = false);

	////////////////////////////////////////////////////
	// WorldWidget
protected:
	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	TArray<FWorldWidgetConfig> WorldWidgetConfigs;

	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	TSubclassOf<UWorldWidgetContainer> WorldWidgetContainerClass;

	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	int32 WorldWidgetContainerZOrder;

	UPROPERTY(EditAnywhere, Category = "WorldWidget")
	TEnumAsByte<ETraceTypeQuery> WorldWidgetTraceType;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, int32> WorldWidgetConfigIndexMap;

	TMultiMap<TObjectPtr<UClass>, FGameplayTag> WorldWidgetClassTagMap;

	UPROPERTY(Transient)
	TMap<FGameplayTag, FWorldWidgets> WorldWidgetByTag;

	UPROPERTY(VisibleAnywhere, Transient, Category = "WorldWidget")
	UWorldWidgetContainer* WorldWidgetContainer;

private:
	const FWorldWidgetConfig* GetWorldWidgetConfig(FGameplayTag InWidgetTag) const;

	FGameplayTag ResolveWorldWidgetTagForClass(TSubclassOf<UWorldWidgetBase> InClass, bool bEnsured = true) const;

protected:
	UFUNCTION()
	void OnSetWorldWidgetVisible(UObject* InSender, const FEventSetWorldWidgetVisible& InEvent);

public:
	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InWidgetTag"))
	TArray<UWorldWidgetBase*> GetWorldWidgetsByTag(FGameplayTag InWidgetTag) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InExpectedClass", AutoCreateRefTerm = "InWidgetTag"))
	UWorldWidgetBase* GetWorldWidgetByTag(FGameplayTag InWidgetTag, int32 InIndex, TSubclassOf<UWorldWidgetBase> InExpectedClass = nullptr) const;

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag,InParams"))
	UWorldWidgetBase* CreateWorldWidgetByTag(FGameplayTag InWidgetTag, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams, TSubclassOf<UWorldWidgetBase> InClassOverride = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"))
	bool DestroyWorldWidgetByTag(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, bool bRecovery = false);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InWidgetTag"))
	bool GetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag) const;

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"))
	void SetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag, bool bVisible);

	UFUNCTION(BlueprintPure)
	UWorldWidgetContainer* GetWorldWidgetContainer() const { return WorldWidgetContainer; }

	UFUNCTION(BlueprintPure)
	ETraceTypeQuery GetWorldWidgetTraceType() const { return WorldWidgetTraceType; }

	UFUNCTION(BlueprintPure)
	bool GetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable)
	void SetWorldWidgetVisible(bool bVisible, TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	template<class T>
	bool HasWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass()) const
	{
		return GetWorldWidgetByTag(ResolveWorldWidgetTagForClass(InClass, false), InIndex, InClass) != nullptr;
	}

	UFUNCTION(BlueprintPure)
	bool HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const;

	template<class T>
	T* GetWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass()) const
	{
		return Cast<T>(GetWorldWidgetByTag(ResolveWorldWidgetTagForClass(InClass), InIndex, InClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldWidgetBase* GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex) const;

	template<class T>
	TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass()) const
	{
		TArray<T*> Result;
		for(UWorldWidgetBase* Widget : GetWorldWidgetsByTag(ResolveWorldWidgetTagForClass(InClass)))
		{
			Result.Add(Cast<T>(Widget));
		}
		return Result;
	}

	UFUNCTION(BlueprintPure)
	TArray<UWorldWidgetBase*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass) const;

	template<class T>
	T* CreateWorldWidget(UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass)
		{
			return nullptr;
		}
		return Cast<T>(CreateWorldWidgetByTag(ResolveWorldWidgetTagForClass(InClass), InOwner, InMapping, InParams ? *InParams : TArray<FParameter>(), InClass));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"))
	UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams);

	bool DestroyWorldWidget(UWorldWidgetBase* InWidget, bool bRecovery = false);

	template<class T>
	bool DestroyWorldWidget(int32 InIndex, bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		const FGameplayTag WidgetTag = ResolveWorldWidgetTagForClass(InClass);
		return DestroyWorldWidgetByTag(WidgetTag, GetWorldWidgetByTag(WidgetTag, InIndex, InClass), bRecovery);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery = false);

	template<class T>
	void DestroyWorldWidgets(bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		const FGameplayTag WidgetTag = ResolveWorldWidgetTagForClass(InClass);
		for(UWorldWidgetBase* Widget : GetWorldWidgetsByTag(WidgetTag))
		{
			DestroyWorldWidgetByTag(WidgetTag, Widget, bRecovery);
		}
	}

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	void ClearAllWorldWidget(bool bRecovery = false);
};
