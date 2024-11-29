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
	template<class T>
	static T* GetTemporaryUserWidget()
	{
		return UWidgetModule::Get().GetTemporaryUserWidget<T>();
	}
	
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetTemporaryUserWidget();

	template<class T>
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().HasUserWidgetClass<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetClassByName(FName InName)
	{
		return UWidgetModule::Get().HasUserWidgetClassByName(InName);
	}

	template<class T>
	static bool GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return UWidgetModule::Get().GetUserWidgetClassByName(WidgetName);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TSubclassOf<UUserWidgetBase> GetUserWidgetClassByName(FName InName)
	{
		return UWidgetModule::Get().GetUserWidgetClassByName(InName);
	}

	template<class T>
	static FName GetUserWidgetParent(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return GetUserWidgetParentByName(WidgetName);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static FName GetUserWidgetParentByName(FName InName)
	{
		return UWidgetModule::Get().GetUserWidgetParentByName(InName);
	}

	template<class T>
	static TArray<FName> GetUserWidgetChildren(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		const FName WidgetName = InClass.GetDefaultObject()->GetWidgetName();
		return GetUserWidgetChildrenByName(WidgetName);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<FName> GetUserWidgetChildrenByName(FName InName)
	{
		return UWidgetModule::Get().GetUserWidgetChildrenByName(InName);
	}

	template<class T>
	static void AddUserWidgetClassMapping(FName InName, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		UWidgetModule::Get().AddUserWidgetClassMapping<T>(InName, InClass);
	}

	UFUNCTION(Blueprintable, Category = "WidgetModuleStatics")
	static void AddUserWidgetClassMapping(FName InName, TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().HasUserWidget<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidget(TSubclassOf<UUserWidgetBase> InClass);

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasUserWidgetByName(FName InName)
	{
		return UWidgetModule::Get().HasUserWidgetByName(InName);
	}

	template<class T>
	static T* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetUserWidget<T>(InClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* GetUserWidget(TSubclassOf<UUserWidgetBase> InClass);

	template<class T>
	static T* GetUserWidgetByName(FName InName)
	{
		return UWidgetModule::Get().GetUserWidgetByName<T>(InName);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	static UUserWidgetBase* GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass = nullptr);
	
	template<class T>
	static T* CreateUserWidget(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CreateUserWidget<T>(InOwner, InParams, bForce, InClass);
	}

	template<class T>
	static T* CreateUserWidget(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return CreateUserWidget<T>(InOwner, &InParams, bForce, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false);

	template<class T>
	static T* CreateUserWidgetByName(FName InName, UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bForce = false)
	{
		return UWidgetModule::Get().CreateUserWidgetByName<T>(InName, InOwner, InParams, bForce);
	}
	
	template<class T>
	static T* CreateUserWidgetByName(FName InName, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false)
	{
		return CreateUserWidgetByName<T>(InName, InOwner, &InParams, bForce);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static UUserWidgetBase* CreateUserWidgetByName(FName InName, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce = false);

	template<class T>
	static bool OpenUserWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().OpenUserWidget<T>(InParams, bInstant, bForce, InClass);
	}

	template<class T>
	static bool OpenUserWidget(const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return OpenUserWidget<T>(&InParams, bInstant, bForce, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static bool OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false);

	static bool OpenUserWidgetByName(FName InName, const TArray<FParameter>* InParams = nullptr, bool bInstant = false, bool bForce = false)
	{
		return UWidgetModule::Get().OpenUserWidgetByName(InName, InParams, bInstant, bForce);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static bool OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant = false, bool bForce = false);

	template<class T>
	static bool CloseUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CloseUserWidget<T>(bInstant, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool CloseUserWidgetByName(FName InName, bool bInstant = false)
	{
		return UWidgetModule::Get().CloseUserWidgetByName(InName, bInstant);
	}

	template<class T>
	static bool ToggleUserWidget(bool bInstant = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().ToggleUserWidget<T>(bInstant, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool ToggleUserWidgetByName(FName InName, bool bInstant = false)
	{
		return UWidgetModule::Get().ToggleUserWidgetByName(InName, bInstant);
	}

	template<class T>
	static bool DestroyUserWidget(bool bRecovery = false, TSubclassOf<UUserWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().DestroyUserWidget<T>(bRecovery, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyUserWidgetByName(FName InName, bool bRecovery = false)
	{
		return UWidgetModule::Get().DestroyUserWidgetByName(InName, bRecovery);
	}

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
	static TSharedPtr<T> CreateSlateWidget(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr)
	{
		return FSlateWidgetManager::Get().CreateSlateWidget<T>(InOwner, InParams);
	}

	template<class T>
	static bool OpenSlateWidget(const TArray<FParameter>* InParams = nullptr, bool bInstant = false, FName InName = T::WidgetName)
	{
		return FSlateWidgetManager::Get().OpenSlateWidget<T>(InParams, bInstant, InName);
	}
	
	template<class T>
	static bool OpenSlateWidget(const TArray<FParameter>& InParams, bool bInstant = false, FName InName = T::WidgetName)
	{
		return FSlateWidgetManager::Get().OpenSlateWidget<T>(&InParams, bInstant, InName);
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
	static bool DestroySlateWidget(bool bRecovery = false, FName InName = T::WidgetName)
	{
		return FSlateWidgetManager::Get().DestroySlateWidget<T>(bRecovery, InName);
	}

	static void CloseAllSlateWidget(bool bInstant = false);
	
	////////////////////////////////////////////////////
	// WorldWidget
	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static UWorldWidgetContainer* GetWorldWidgetContainer();

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool GetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void SetWorldWidgetVisible(bool bVisible, TSubclassOf<UWorldWidgetBase> InClass = nullptr);

	template<class T>
	static bool HasWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().HasWorldWidget<T>(InIndex, InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static bool HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	UFUNCTION(BlueprintPure)
	static bool HasWorldWidgetByName(FName InName, int32 InIndex)
	{
		return UWidgetModule::Get().HasWorldWidgetByName(InName, InIndex);
	}

	template<class T>
	static T* GetWorldWidget(int32 InIndex, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetWorldWidget<T>(InIndex, InClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	template<class T>
	static T* GetWorldWidgetByName(FName InName, int32 InIndex)
	{
		return UWidgetModule::Get().GetWorldWidgetByName<T>(InName, InIndex);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex);

	template<class T>
	static TArray<T*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().GetWorldWidgets<T>(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass);

	template<class T>
	static TArray<T*> GetWorldWidgetsByName(FName InName)
	{
		return UWidgetModule::Get().GetWorldWidgetsByName<T>(InName);
	}

	UFUNCTION(BlueprintPure, Category = "WidgetModuleStatics")
	static TArray<UWorldWidgetBase*> GetWorldWidgetsByName(FName InName);

	template<class T>
	static T* CreateWorldWidget(UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().CreateWorldWidget<T>(InOwner, InMapping, InParams, InClass);
	}

	template<class T>
	static T* CreateWorldWidget(UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return CreateWorldWidget<T>(InOwner, InMapping, &InParams, InClass);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams);

	template<class T>
	static T* CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>* InParams = nullptr)
	{
		return UWidgetModule::Get().CreateWorldWidgetByName<T>(InName, InOwner, InMapping, InParams);
	}

	template<class T>
	static T* CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
	{
		return CreateWorldWidgetByName<T>(InName, InOwner, InMapping, &InParams);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"), Category = "WidgetModuleStatics")
	static UWorldWidgetBase* CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams);

	static bool DestroyWorldWidget(UWorldWidgetBase* InWidget, bool bRecovery = false)
	{
		if(!InWidget) return false;

		const FName WidgetName = InWidget->GetWidgetName();
		
		return DestroyWorldWidgetByName(WidgetName, InWidget->GetWidgetIndex(), bRecovery);
	}

	template<class T>
	static bool DestroyWorldWidget(int32 InIndex, bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		return UWidgetModule::Get().DestroyWorldWidget<T>(InIndex, bRecovery, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery = false);

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static bool DestroyWorldWidgetByName(FName InName, int32 InIndex, bool bRecovery = false)
	{
		return UWidgetModule::Get().DestroyWorldWidgetByName(InName, InIndex, bRecovery);
	}

	template<class T>
	static void DestroyWorldWidgets(bool bRecovery = false, TSubclassOf<UWorldWidgetBase> InClass = T::StaticClass())
	{
		UWidgetModule::Get().DestroyWorldWidgets<T>(bRecovery, InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "WidgetModuleStatics")
	static void DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery = false);

	UFUNCTION(BlueprintCallable)
	static void DestroyWorldWidgetsByName(FName InName, bool bRecovery = false)
	{
		UWidgetModule::Get().DestroyWorldWidgetsByName(InName, bRecovery);
	}
};
