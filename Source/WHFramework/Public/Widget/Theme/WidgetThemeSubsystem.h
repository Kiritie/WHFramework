#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "Widget/Theme/WidgetTheme.h"
#include "WidgetThemeSubsystem.generated.h"
struct FStreamableHandle;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWidgetThemeSwitchResult, bool, bSuccess, FString, Error);

UCLASS()
class WHFRAMEWORK_API UWidgetThemeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	friend class FWidgetThemeSwitchOwnershipTest;

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
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
