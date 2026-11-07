#pragma once

#include "Widget/ScreenWidgetTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widget/WidgetModuleTypes.h"
#include "Widget/Style/CommonStyleTypes.h"
#include "SettingPresentationTypes.generated.h"

UENUM(BlueprintType)
enum class ESettingRendererType : uint8
{
	Auto,
	Bool,
	Number,
	Enum,
	Text,
	Option,
	Key,
	Custom
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingNumberDisplay
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasMin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Min = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasMax = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Max = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Step = 0.01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Scale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DecimalPlaces = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Suffix;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingPageDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Page;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "UI.Style.Text"))
	FGameplayTag LabelTextStyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "Optional text overrides keyed by the entry renderer name, such as Bool, Enum, Option or Key."))
	TMap<FName, FGameplayTag> RendererLabelTextStyleTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCommonControlStyleTags ControlStyles;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetPressAnyKeyOpenParameter : public FWidgetOpenParameter
{
	GENERATED_BODY()

public:
	FWidgetPressAnyKeyOpenParameter() = default;

	explicit FWidgetPressAnyKeyOpenParameter(int32 InKeyIndex)
		: KeyIndex(InKeyIndex)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 KeyIndex = INDEX_NONE;
};
