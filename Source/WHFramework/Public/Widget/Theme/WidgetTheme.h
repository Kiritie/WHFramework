#pragma once

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Setting/SettingModuleTypes.h"

#include "Widget/Style/WidgetBrushStyle.h"
#include "Widget/Theme/WidgetStyleCatalog.h"
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
	TSubclassOf<UCommonButtonStyle> Style;
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
	TSubclassOf<UWidgetBrushStyle> Style;
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
	static FWidgetThemeDuplicateDelegate DuplicateStyles;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	bool bRuntimeTheme = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bDuplicateFailed = false;
	UPROPERTY()
	TMap<FGameplayTag, FWidgetStyleBinding> Bindings;
#endif
	UClass* GetStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind) const;
#if WITH_EDITOR
	bool SetStyleClass(FGameplayTag Tag, EWidgetStyleKind Kind, UClass* StyleClass);
	void RemoveStyle(FGameplayTag Tag, EWidgetStyleKind Kind);
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "Style.Button"))
	TMap<FGameplayTag, FWidgetButtonStyleData> ButtonStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "Style.Text"))
	TMap<FGameplayTag, FWidgetTextStyleData> TextStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "Style.Brush"))
	TMap<FGameplayTag, FWidgetBrushStyleData> BrushStyles;

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

	const FSettingRendererClasses& GetSettingRendererClasses() const
	{
		return SettingRendererClasses;
	}

	TSubclassOf<UWidgetSettingCategoryEntry> GetSettingCategoryEntryClass() const
	{
		return SettingCategoryEntryClass;
	}

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
