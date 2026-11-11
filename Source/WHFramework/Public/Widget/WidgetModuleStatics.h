// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Widget/WorldWidgetTypes.h"
#include "Common/LanguageTypes.h"
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
	static TArray<FLanguageType> GetWidgetLanguageTypes(const UObject* InWorldContext);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWidgetLanguageTypes(const UObject* InWorldContext, const TArray<FLanguageType>& InLanguageTypes);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static int32 GetWidgetLanguageType(const UObject* InWorldContext);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWidgetLanguageType(const UObject* InWorldContext, int32 InLanguageType);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static float GetWidgetGlobalScale(const UObject* InWorldContext);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWidgetGlobalScale(const UObject* InWorldContext, float InGlobalScale);

	////////////////////////////////////////////////////
	// CommonWidget
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UDataTable* GetCommonRichTextStyle(const UObject* InWorldContext);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetCommonRichTextStyle(const UObject* InWorldContext, UDataTable* InCommonRichTextStyle);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<TSubclassOf<URichTextBlockDecorator>> GetCommonRichTextDecorators(const UObject* InWorldContext);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetCommonRichTextDecorators(const UObject* InWorldContext, const TArray<TSubclassOf<URichTextBlockDecorator>>& InCommonRichTextDecorators);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void AddCommonRichTextDecorator(const UObject* InWorldContext, const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void RemoveCommonRichTextDecorator(const UObject* InWorldContext, const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator);

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UCommonToolTip* SpawnToolTipForWidget(UWidget* InWidget, TSubclassOf<UCommonToolTip> InToolTipClass);

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static void DestroyToolTipForWidget(UWidget* InWidget);

	////////////////////////////////////////////////////
	// ScreenWidget
	static UWidgetModule* GetPlayerModule(APlayerController* InPlayer);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InExpectedClass", AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, TSubclassOf<UUserWidgetBase> InExpectedClass = nullptr);
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InWidgetTag,InParam"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, const FParameter& InParam = FParameter(), TSubclassOf<UUserWidgetBase> InClass = nullptr);
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag,InParam"), Category = "WidgetModuleStatics")
	static bool OpenUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false);
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool CloseUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, bool bInstant = false);
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool ToggleUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, bool bInstant = false);
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool DestroyUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	template<class T> static bool HasUserWidgetClass(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module && Module->HasUserWidgetClass<T>(InClass); }
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetClass(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass);
	template<class T> static TSubclassOf<UUserWidgetBase> GetUserWidgetClass(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module ? Module->GetUserWidgetClass<T>(InClass) : nullptr; }
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TSubclassOf<UUserWidgetBase> GetUserWidgetClass(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass);
	template<class T> static bool HasUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module && Module->HasUserWidget<T>(InPlayer, InClass); }
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass);
	template<class T> static T* GetUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module ? Module->GetUserWidget<T>(InPlayer, InClass) : nullptr; }
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass);
	template<class T> static T* CreateUserWidget(APlayerController* InPlayer, const FParameter& InParam = FParameter(), TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module ? Module->CreateUserWidget<T>(InPlayer, InParam, InClass) : nullptr; }
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParam"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam = FParameter());
	template<class T> static bool OpenUserWidget(APlayerController* InPlayer, const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module && Module->OpenUserWidget<T>(InPlayer, InParam, bInstant, bForce, InClass); }
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParam"), Category = "WidgetModuleStatics")
	static bool OpenUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam = FParameter(), bool bInstant = false, bool bForce = false);
	template<class T> static bool CloseUserWidget(APlayerController* InPlayer, bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module && Module->CloseUserWidget<T>(InPlayer, bInstant, InClass); }
	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool CloseUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);
	template<class T> static bool ToggleUserWidget(APlayerController* InPlayer, bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module && Module->ToggleUserWidget<T>(InPlayer, bInstant, InClass); }
	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool ToggleUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);
	template<class T> static bool DestroyUserWidget(APlayerController* InPlayer, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{ UWidgetModule* Module = GetPlayerModule(InPlayer); return Module && Module->DestroyUserWidget<T>(InPlayer, InMode, InClass); }
	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);
	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void CloseAllUserWidget(APlayerController* InPlayer, bool bInstant = false);

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
	static TArray<UWorldWidgetBase*> GetWorldWidgetsByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag,InParam"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* CreateWorldWidgetByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag, FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter(), TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool DestroyWorldWidgetByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static bool GetWorldWidgetVisibleByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InWidgetTag"), Category = "WidgetModuleStatics")
	static void SetWorldWidgetVisibleByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag, bool bVisible);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UWorldWidgetContainer* GetWorldWidgetContainer(APlayerController* InPlayer);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool GetWorldWidgetVisible(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWorldWidgetVisible(const UObject* InWorldContext, bool bVisible, TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	template<class T>
	static TArray<T*> GetWorldWidgets(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		UWidgetModule* Widgets = InWorldContext ? UWidgetModule::GetPtr(InWorldContext->GetWorld()) : nullptr;
		return Widgets ? Widgets->GetWorldWidgets<T>(InClass) : TArray<T*>();
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgets(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass);

	template<class T>
	static T* CreateWorldWidget(const UObject* InWorldContext, FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter(), TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		UWidgetModule* Widgets = InWorldContext ? UWidgetModule::GetPtr(InWorldContext->GetWorld()) : nullptr;
		return Widgets ? Widgets->CreateWorldWidget<T>(InMapping, InParam, InClass) : nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParam"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* CreateWorldWidget(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass, FWorldWidgetMapping InMapping, const FParameter& InParam = FParameter());

	static bool DestroyWorldWidget(UWorldWidgetBase* InWidget, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy)
	{
		UWidgetModule* Widgets = InWidget ? UWidgetModule::GetPtr(InWidget->GetWorld()) : nullptr;
		return Widgets && Widgets->DestroyWorldWidget(InWidget, InMode);
	}

	template<class T>
	static void DestroyWorldWidgets(const UObject* InWorldContext, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		if (UWidgetModule* Widgets = InWorldContext ? UWidgetModule::GetPtr(InWorldContext->GetWorld()) : nullptr) Widgets->DestroyWorldWidgets<T>(InMode, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void DestroyWorldWidgets(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass, EObjectDespawnMode InMode = EObjectDespawnMode::Destroy);
};
