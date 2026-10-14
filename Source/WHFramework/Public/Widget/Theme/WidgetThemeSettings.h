#pragma once
#include "Engine/DeveloperSettings.h"
#include "Widget/Theme/WidgetTheme.h"
#include "WidgetThemeSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Widget Theme"))
class WHFRAMEWORK_API UWidgetThemeSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category = "Themes")
	TSoftObjectPtr<UWidgetTheme> DefaultTheme;
	UPROPERTY(Config, EditAnywhere, Category = "Themes")
	TSoftObjectPtr<UWidgetStyleCatalog> StyleCatalog;
	UPROPERTY(Config, EditAnywhere, Category = "Themes")
	TArray<TSoftObjectPtr<UWidgetTheme>> AvailableThemes;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

	UPROPERTY(Config, EditAnywhere, Category = "Authoring")
	TArray<FName> AllowedFrameworkNativeModules = {
		TEXT("CoreUObject"),	TEXT("Engine"),		  TEXT("UMG"),		   TEXT("SlateCore"),		TEXT("CommonUI"),		  TEXT("CommonInput"),
		TEXT("InputCore"),		TEXT("GameplayTags"), TEXT("WHFramework"), TEXT("WHFrameworkCore"), TEXT("WHFrameworkSlate"), TEXT("WHFrameworkEditor"),
		TEXT("BlueprintGraph"), TEXT("UnrealEd"),	  TEXT("Kismet"),	   TEXT("KismetCompiler"),	TEXT("UMGEditor")};
	UPROPERTY(Config, EditAnywhere, Category = "Authoring")
	TArray<FString> AllowedFrameworkPluginRoots = {
		TEXT("/CommonUI"),	  TEXT("/EnhancedInput"),		TEXT("/GameplayAbilities"), TEXT("/Paper2D"), TEXT("/ProceduralMeshComponent"),
		TEXT("/StructUtils"), TEXT("/WHFrameworkUtilities")};
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}
};
