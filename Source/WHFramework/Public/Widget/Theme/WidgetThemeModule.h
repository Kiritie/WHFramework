#pragma once
#include "Common/LifecycleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Widget/Theme/WidgetTheme.h"
#include "WidgetThemeModule.generated.h"
struct FStreamableHandle;
class UGameInstance;
class UWorld;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWidgetThemeSwitchResult, bool, bSuccess, FString, Error);

UCLASS()
class WHFRAMEWORK_API UWidgetThemeModule : public UModuleBase
{
	GENERATED_BODY()
	friend class FWidgetThemeSwitchOwnershipTest;

public:
	UWidgetThemeModule();
	virtual EModuleScope GetModuleScope() const override { return EModuleScope::GameInstance; }
	virtual bool ShouldCreateForGameInstance(const UWHGameInstance* InGameInstance) const override;
	virtual void OnInitialize() override;
	virtual void OnTermination(EPhase InPhase) override;
	static UWidgetThemeModule* GetPtr(const UGameInstance* InGameInstance);
	static UWidgetThemeModule* GetPtr(const UWorld* InWorld);
	UFUNCTION(BlueprintCallable, Category = "Theme")
	void RequestTheme(TSoftObjectPtr<UWidgetTheme> Theme);
	UFUNCTION(BlueprintPure, Category = "Theme")
	UWidgetTheme* GetActiveTheme() const
	{
		return ActiveTheme;
	}

	bool ApplyLoadedTheme(UWidgetTheme* Theme, FString& Error);
	UPROPERTY(BlueprintAssignable, Category = "Theme")
	FWidgetThemeSwitchResult OnSwitchFinished;

private:
	void CompleteRequest(uint64 Serial, TSoftObjectPtr<UWidgetTheme> Requested);
	UPROPERTY(Transient)
	TObjectPtr<UWidgetTheme> ActiveTheme;
	TSharedPtr<FStreamableHandle> Pending;
	uint64 RequestSerial = 0;
};
