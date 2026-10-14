#include "Widget/Theme/WidgetThemeValidationCommandlet.h"

#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

UWidgetThemeValidationCommandlet::UWidgetThemeValidationCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UWidgetThemeValidationCommandlet::Main(const FString& Params)
{
	FString Report;
	FString Inventory;
	if (FParse::Value(*Params, TEXT("Inventory="), Inventory))
	{
		const bool Good = FWidgetThemeEditorService::ExportInventory(Inventory, Report);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
		if (FParse::Param(*Params, TEXT("InventoryOnly")))
		{
			return Good ? 0 : 1;
		}
	}
	auto* Settings = GetMutableDefault<UWidgetThemeSettings>();
	TArray<TSoftObjectPtr<UWidgetTheme>> Required = Settings->AvailableThemes;
	Required.AddUnique(Settings->DefaultTheme);

	bool Good = !Settings->DefaultTheme.IsNull();
	for (const auto& Path : Required)
	{
		UWidgetTheme* Theme = Path.LoadSynchronous();
		FString Result;
		if (!Theme)
		{
			Good = false;
			Report += TEXT("Missing configured theme: ") + Path.ToString() + TEXT("\n");
			continue;
		}

		if (FParse::Param(*Params, TEXT("Organize")))
		{
			Good &= FWidgetThemeEditorService::OrganizeAssets(Theme, Result);
		}
		if (FParse::Param(*Params, TEXT("Organize")))
		{
			Good &= FWidgetThemeEditorService::Save(Theme, Result);
		}
		FString RuntimeResult;
		const bool Valid = FWidgetThemeEditorService::Validate(Theme, Result) &&
						   FWidgetThemeResolver::Validate(Theme, FWidgetThemeEditorService::Catalog(), RuntimeResult, Theme->bRuntimeTheme);
		Result += RuntimeResult;
		Good &= Valid;
		Report += Theme->GetPathName() + (Valid ? TEXT(": VALID\n") : TEXT(": INVALID\n")) + Result;
	}
	if (FParse::Param(*Params, TEXT("Organize")))
	{
		Settings->DefaultTheme = Settings->DefaultTheme.LoadSynchronous();
		Settings->StyleCatalog = Settings->StyleCatalog.LoadSynchronous();
		for (auto& Path : Settings->AvailableThemes)
		{
			Path = Path.LoadSynchronous();
		}
		Good &= Settings->TryUpdateDefaultConfigFile();
	}
	FString Output;
	if (FParse::Value(*Params, TEXT("Report="), Output))
	{
		Good &= FFileHelper::SaveStringToFile(Report, *Output);
	}
	UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
	return Good ? 0 : 1;
}
