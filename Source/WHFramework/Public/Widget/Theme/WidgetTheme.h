#pragma once

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Engine/DataAsset.h"
#include "CommonBorder.h"
#include "Widget/Style/CommonInputStyle.h"
#include "Widget/Style/CommonToggleStyle.h"
#include "Widget/Style/CommonRangeStyle.h"
#include "Widget/Style/CommonProgressStyle.h"
#include "Widget/Style/CommonWindowStyle.h"
#include "Widget/Style/CommonScrollStyle.h"
#include "Widget/Style/CommonButtonStyleN.h"

#include "GameplayTagContainer.h"
#include "Setting/SettingModuleTypes.h"

#include "Widget/Style/CommonBrushStyle.h"
#include "Widget/Theme/WidgetStyleTypes.h"
#include "WidgetTheme.generated.h"

class UWidgetSettingEntryBase;
class UWidgetSettingCategoryEntry;
class UWidgetTheme;

#if WITH_EDITOR
DECLARE_DELEGATE_RetVal_OneParam(bool, FWidgetThemeDuplicateDelegate, UWidgetTheme*);
#endif

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingRendererClasses
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TSubclassOf<UWidgetSettingEntryBase> BoolClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TSubclassOf<UWidgetSettingEntryBase> NumberClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TSubclassOf<UWidgetSettingEntryBase> EnumClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TSubclassOf<UWidgetSettingEntryBase> TextClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TSubclassOf<UWidgetSettingEntryBase> OptionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TSubclassOf<UWidgetSettingEntryBase> KeyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TSubclassOf<UWidgetSettingEntryBase> CustomClass;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetButtonStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonButtonStyleN> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetTextStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonTextStyle> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetBrushStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FSlateBrush Brush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonBrushStyle> Style;
};


USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetInputStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonInputStyle> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetToggleStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonToggleStyle> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetRangeStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonRangeStyle> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetProgressStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonProgressStyle> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetScrollStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonScrollStyle> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetBorderStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonBorderStyle> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetTextScrollStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonTextScrollStyle> Style;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetWindowStyleData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSubclassOf<UCommonWindowStyle> Style;
};

USTRUCT()
struct WHFRAMEWORK_API FWidgetStyleBinding
{
	GENERATED_BODY()
	UPROPERTY()
	TObjectPtr<UWidgetTheme> SourceTheme;
	UPROPERTY()
	FGameplayTag SourceTag;
	UPROPERTY()
	FGuid AssetId;
	UPROPERTY()
	bool bManaged = false;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetTheme : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWidgetTheme();

#if WITH_EDITOR
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
	static FWidgetThemeDuplicateDelegate DuplicateStyles;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	bool bRuntimeTheme = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	TObjectPtr<UWidgetTheme> ParentTheme;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bDuplicateFailed = false;
	UPROPERTY()
	TMap<FGameplayTag, FWidgetStyleBinding> Bindings;
#endif
	UClass* GetStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind) const;
	bool HasLocalStyle(FGameplayTag Tag, EWidgetStyleKind Kind) const;
	UClass* ResolveStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind) const;
	TArray<FGameplayTag> GetStyleTags(EWidgetStyleKind Kind, bool bIncludeInherited = false) const;

#if WITH_EDITOR
	bool SetStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind, UClass* StyleClass);
	void RemoveStyle(FGameplayTag Tag, EWidgetStyleKind Kind);
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Button"))
	TMap<FGameplayTag, FWidgetButtonStyleData> ButtonStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Text"))
	TMap<FGameplayTag, FWidgetTextStyleData> TextStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Brush"))
	TMap<FGameplayTag, FWidgetBrushStyleData> BrushStyles;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Input"))
	TMap<FGameplayTag, FWidgetInputStyleData> InputStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Toggle"))
	TMap<FGameplayTag, FWidgetToggleStyleData> ToggleStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Range"))
	TMap<FGameplayTag, FWidgetRangeStyleData> RangeStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Progress"))
	TMap<FGameplayTag, FWidgetProgressStyleData> ProgressStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Scroll"))
	TMap<FGameplayTag, FWidgetScrollStyleData> ScrollStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Border"))
	TMap<FGameplayTag, FWidgetBorderStyleData> BorderStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.TextScroll"))
	TMap<FGameplayTag, FWidgetTextScrollStyleData> TextScrollStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "UI.Style.Window"))
	TMap<FGameplayTag, FWidgetWindowStyleData> WindowStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	FSettingRendererClasses SettingRendererClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	TSubclassOf<UWidgetSettingCategoryEntry> SettingCategoryEntryClass;

public:
	const TMap<FGameplayTag, FWidgetButtonStyleData>& GetButtonStyles() const
	{
		return ButtonStyles;
	}

	const TMap<FGameplayTag, FWidgetTextStyleData>& GetTextStyles() const
	{
		return TextStyles;
	}

	const TMap<FGameplayTag, FWidgetBrushStyleData>& GetBrushStyles() const
	{
		return BrushStyles;
	}


	const TMap<FGameplayTag, FWidgetInputStyleData>& GetInputStyles() const
	{
		return InputStyles;
	}

	const TMap<FGameplayTag, FWidgetToggleStyleData>& GetToggleStyles() const
	{
		return ToggleStyles;
	}

	const TMap<FGameplayTag, FWidgetRangeStyleData>& GetRangeStyles() const
	{
		return RangeStyles;
	}

	const TMap<FGameplayTag, FWidgetProgressStyleData>& GetProgressStyles() const
	{
		return ProgressStyles;
	}

	const TMap<FGameplayTag, FWidgetScrollStyleData>& GetScrollStyles() const
	{
		return ScrollStyles;
	}

	const TMap<FGameplayTag, FWidgetBorderStyleData>& GetBorderStyles() const
	{
		return BorderStyles;
	}

	const TMap<FGameplayTag, FWidgetTextScrollStyleData>& GetTextScrollStyles() const
	{
		return TextScrollStyles;
	}

	const TMap<FGameplayTag, FWidgetWindowStyleData>& GetWindowStyles() const
	{
		return WindowStyles;
	}

	const FSettingRendererClasses& GetSettingRendererClasses() const
	{
		return SettingRendererClasses;
	}

	TSubclassOf<UWidgetSettingCategoryEntry> GetSettingCategoryEntryClass() const;

	const FWidgetButtonStyleData* FindButtonStyle(FGameplayTag InStyleTag) const;

	const FWidgetTextStyleData* FindTextStyle(FGameplayTag InStyleTag) const;

	const FWidgetBrushStyleData* FindBrushStyle(FGameplayTag InStyleTag) const;

	TSubclassOf<UWidgetSettingEntryBase> FindSettingRendererClass(ESettingRendererType InRenderer) const;

#if WITH_EDITOR
	bool AreStylesSorted() const;

	void SortStyles();

	void SetButtonStyles(const TMap<FGameplayTag, FWidgetButtonStyleData>& InStyles)
	{
		ButtonStyles = InStyles;
	}

	void SetTextStyles(const TMap<FGameplayTag, FWidgetTextStyleData>& InStyles)
	{
		TextStyles = InStyles;
	}

	void SetBrushStyles(const TMap<FGameplayTag, FWidgetBrushStyleData>& InStyles)
	{
		BrushStyles = InStyles;
	}

	void SetSettingRendererClasses(const FSettingRendererClasses& InClasses)
	{
		SettingRendererClasses = InClasses;
	}
#endif
};
