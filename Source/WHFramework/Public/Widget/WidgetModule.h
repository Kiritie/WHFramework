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
class UWidgetTheme;
struct FEventSetWorldWidgetVisible;
struct FEventCloseUserWidget;
struct FEventOpenUserWidget;

UCLASS()
class WHFRAMEWORK_API UWidgetModule : public UModuleBase
{
	friend class UWorldWidgetComponent;

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
	// WidgetTheme
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CommonWidget|Theme")
	TObjectPtr<UWidgetTheme> DefaultWidgetTheme;

public:
	UFUNCTION(BlueprintPure)
	UWidgetTheme* GetDefaultWidgetTheme() const { return DefaultWidgetTheme; }

	UFUNCTION(BlueprintCallable)
	void SetDefaultWidgetTheme(UWidgetTheme* InTheme) { DefaultWidgetTheme = InTheme; }

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

	TSet<FGameplayTag> PendingWidgetOpenTags;

private:
	void BuildRuntimeCaches();

#if WITH_EDITOR
	bool ValidateScreenWidgetConfigs(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	bool ValidateWorldWidgetConfigs(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	bool ValidateWidgetTheme(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	void ValidateNativeWidgetUsage(TArray<FText>& OutWarnings) const;
#endif

	FGameplayTag ResolveWidgetTagForClass(
		TSubclassOf<UUserWidgetBase> InClass,
		bool bEnsured = true) const;

	bool EnsureParentCreated(
		const FScreenWidgetConfig& InConfig,
		const FParameter& InParam);

	bool MountUserWidget(
		UUserWidgetBase* InWidget,
		const FScreenWidgetConfig& InConfig);

	void UnMountUserWidget(
		UUserWidgetBase* InWidget);

public:
	const FScreenWidgetConfig* GetUserWidgetConfig(FGameplayTag InWidgetTag) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InExpectedClass", AutoCreateRefTerm = "InWidgetTag"))
	UUserWidgetBase* GetUserWidgetByTag(
		FGameplayTag InWidgetTag,
		TSubclassOf<UUserWidgetBase> InExpectedClass = nullptr) const;

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InWidgetTag,InParam"))
	UUserWidgetBase* CreateUserWidgetByTag(
		FGameplayTag InWidgetTag,
		const FParameter& InParam = FParameter(),
		TSubclassOf<UUserWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag,InParam"))
	bool OpenUserWidgetByTag(
		FGameplayTag InWidgetTag,
		const FParameter& InParam = FParameter(),
		bool bInstant = false,
		bool bForce = false,
		TSubclassOf<UUserWidgetBase> InClass = nullptr);

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
		EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	UFUNCTION(BlueprintCallable)
	bool CloseActiveTemporaryWidget(
		const FWidgetMountContext& InContext,
		bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParentWidgetTag,InSlotTag"))
	bool CloseTemporaryWidgetInSlot(
		FGameplayTag InParentWidgetTag,
		FGameplayTag InSlotTag,
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
	T* CreateUserWidget(const FParameter& InParam = FParameter(), TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass)
		{
			return nullptr;
		}
		return Cast<T>(CreateUserWidgetByTag(ResolveWidgetTagForClass(InClass), InParam));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParam"))
	UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam = FParameter());

	template<class T>
	bool OpenUserWidget(const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass)
		{
			return false;
		}
		return OpenUserWidgetByTag(
			ResolveWidgetTagForClass(InClass),
			InParam,
			bInstant,
			bForce);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParam"))
	bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false);

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
	bool DestroyUserWidget(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return InClass && DestroyUserWidgetByTag(ResolveWidgetTagForClass(InClass), InMode);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	UFUNCTION(BlueprintCallable)
	void CloseAllUserWidget(bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void ClearAllUserWidget(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

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
	FGameplayTag ResolveWorldWidgetTagForClass(TSubclassOf<UWorldWidgetBase> InClass, bool bEnsured = true) const;

protected:
	UFUNCTION()
	void OnSetWorldWidgetVisible(UObject* InSender, const FEventSetWorldWidgetVisible& InEvent);

public:
	const FWorldWidgetConfig* GetWorldWidgetConfig(FGameplayTag InWidgetTag) const;

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InWidgetTag"))
	TArray<UWorldWidgetBase*> GetWorldWidgetsByTag(FGameplayTag InWidgetTag) const;

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag,InParam"))
	UWorldWidgetBase* CreateWorldWidgetByTag(FGameplayTag InWidgetTag, FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter(), TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"))
	bool DestroyWorldWidgetByTag(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

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
	T* CreateWorldWidget(FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter(), TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if(!InClass)
		{
			return nullptr;
		}
		return Cast<T>(CreateWorldWidgetByTag(ResolveWorldWidgetTagForClass(InClass), InMapping, InParam, InClass));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParam"))
	UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter());

	bool DestroyWorldWidget(UWorldWidgetBase* InWidget, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	template<class T>
	void DestroyWorldWidgets(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		const FGameplayTag WidgetTag = ResolveWorldWidgetTagForClass(InClass);
		for(UWorldWidgetBase* Widget : GetWorldWidgetsByTag(WidgetTag))
		{
			DestroyWorldWidgetByTag(WidgetTag, Widget, InMode);
		}
	}

	UFUNCTION(BlueprintCallable)
	void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	UFUNCTION(BlueprintCallable)
	void ClearAllWorldWidget(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);
};
