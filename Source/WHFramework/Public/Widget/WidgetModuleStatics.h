// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "WidgetModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Slate/SlateWidgetManager.h"
#include "WidgetModuleStatics.generated.h"

class UCommonToolTip;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	////////////////////////////////////////////////////
	// GlobalSettings
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<FLanguageType> GetWidgetLanguageTypes();

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWidgetLanguageTypes(const TArray<FLanguageType>& InLanguageTypes);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static int32 GetWidgetLanguageType();

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWidgetLanguageType(int32 InLanguageType);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static float GetWidgetGlobalScale();

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWidgetGlobalScale(float InGlobalScale);

	////////////////////////////////////////////////////
	// CommonWidget
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UDataTable* GetCommonRichTextStyle();

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetCommonRichTextStyle(UDataTable* InCommonRichTextStyle);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<TSubclassOf<URichTextBlockDecorator>> GetCommonRichTextDecorators();

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetCommonRichTextDecorators(const TArray<TSubclassOf<URichTextBlockDecorator>>& InCommonRichTextDecorators);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void AddCommonRichTextDecorator(const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void RemoveCommonRichTextDecorator(const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator);

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UCommonToolTip* SpawnToolTipForWidget(UWidget* InWidget, TSubclassOf<UCommonToolTip> InToolTipClass);

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static void DestroyToolTipForWidget(UWidget* InWidget);

	////////////////////////////////////////////////////
	// ScreenWidget
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InExpectedClass", AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetUserWidgetByTag(FGameplayTag InWidgetTag, TSubclassOf<UUserWidgetBase> InExpectedClass = nullptr);

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InWidgetTag,InParam"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidgetByTag(FGameplayTag InWidgetTag, const FParameter& InParam = FParameter(), TSubclassOf<UUserWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag,InParam"), Category = "WidgetModuleStatics")
	static bool OpenUserWidgetByTag(FGameplayTag InWidgetTag, const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool CloseUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool ToggleUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant = false);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool DestroyUserWidgetByTag(FGameplayTag InWidgetTag, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	template<class T>
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().HasUserWidgetClass<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static TSubclassOf<UUserWidgetBase> GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetUserWidgetClass<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TSubclassOf<UUserWidgetBase> GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().HasUserWidget<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static T* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetUserWidget<T>(InClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static T* CreateUserWidget(const FParameter& InParam = FParameter(), TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CreateUserWidget<T>(InParam, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParam"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam = FParameter());

	template<class T>
	static bool OpenUserWidget(const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().OpenUserWidget<T>(InParam, bInstant, bForce, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParam"), Category = "WidgetModuleStatics")
	static bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false);

	template<class T>
	static bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CloseUserWidget<T>(bInstant, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	template<class T>
	static bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().ToggleUserWidget<T>(bInstant, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	template<class T>
	static bool DestroyUserWidget(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().DestroyUserWidget<T>(InMode, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void CloseAllUserWidget(bool bInstant = false);

	////////////////////////////////////////////////////
	// SlateWidget
	template<class T>
	static TSharedPtr<T> GetSlateWidget()
	{
		return FSlateWidgetManager::Get().GetSlateWidget<T>();
	}

	template<class T>
	static TSharedPtr<T> CreateSlateWidget(const FParameter& InParam = FParameter())
	{
		return FSlateWidgetManager::Get().CreateSlateWidget<T>(InParam);
	}

	template<class T>
	static bool OpenSlateWidget(const FParameter& InParam = FParameter(), bool bInstant = false, FName InName = T::WidgetName)
	{
		return FSlateWidgetManager::Get().OpenSlateWidget<T>(InParam, bInstant, InName);
	}

	template<class T>
	static bool CloseSlateWidget(bool bInstant = false, FName InName = T::WidgetName)
	{
		return FSlateWidgetManager::Get().CloseSlateWidget<T>(bInstant, InName);
	}
	
	template<class T>
	static bool ToggleSlateWidget(bool bInstant = false, FName InName = T::WidgetName)
	{
		return FSlateWidgetManager::Get().ToggleSlateWidget<T>(bInstant, InName);
	}

	template<class T>
	static bool DestroySlateWidget(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, FName InName = T::WidgetName)
	{
		return FSlateWidgetManager::Get().DestroySlateWidget<T>(InMode, InName);
	}

	static void CloseAllSlateWidget(bool bInstant = false);
	
	////////////////////////////////////////////////////
	// WorldWidget
	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgetsByTag(FGameplayTag InWidgetTag);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag,InParam"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* CreateWorldWidgetByTag(FGameplayTag InWidgetTag, FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter(), TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool DestroyWorldWidgetByTag(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool GetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static void SetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag, bool bVisible);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UWorldWidgetContainer* GetWorldWidgetContainer();

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool GetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWorldWidgetVisible(bool bVisible, TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	template<class T>
	static TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetWorldWidgets<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass);

	template<class T>
	static T* CreateWorldWidget(FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter(), TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CreateWorldWidget<T>(InMapping, InParam, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParam"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter());

	static bool DestroyWorldWidget(UWorldWidgetBase* InWidget, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy)
	{
		return UWidgetModule::Get().DestroyWorldWidget(InWidget, InMode);
	}

	template<class T>
	static void DestroyWorldWidgets(EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		UWidgetModule::Get().DestroyWorldWidgets<T>(InMode, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);
};
