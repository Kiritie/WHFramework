#pragma once

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Setting/SettingModuleTypes.h"

#include "WidgetTheme.generated.h"

class UWidgetSettingEntryBase;

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
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetTheme : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWidgetTheme();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "Style.Button"))
	TMap<FGameplayTag, FWidgetButtonStyleData> ButtonStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "Style.Text"))
	TMap<FGameplayTag, FWidgetTextStyleData> TextStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "Style.Brush"))
	TMap<FGameplayTag, FWidgetBrushStyleData> BrushStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting")
	FSettingRendererClasses SettingRendererClasses;

public:
	const TMap<FGameplayTag, FWidgetButtonStyleData>& GetButtonStyles() const { return ButtonStyles; }

	const TMap<FGameplayTag, FWidgetTextStyleData>& GetTextStyles() const { return TextStyles; }

	const TMap<FGameplayTag, FWidgetBrushStyleData>& GetBrushStyles() const { return BrushStyles; }

	const FSettingRendererClasses& GetSettingRendererClasses() const { return SettingRendererClasses; }

	const FWidgetButtonStyleData* FindButtonStyle(FGameplayTag InStyleTag) const;

	const FWidgetTextStyleData* FindTextStyle(FGameplayTag InStyleTag) const;

	const FWidgetBrushStyleData* FindBrushStyle(FGameplayTag InStyleTag) const;

	TSubclassOf<UWidgetSettingEntryBase> FindSettingRendererClass(ESettingRendererType InRenderer) const;

#if WITH_EDITOR
	void SetButtonStyles(const TMap<FGameplayTag, FWidgetButtonStyleData>& InStyles) { ButtonStyles = InStyles; }

	void SetTextStyles(const TMap<FGameplayTag, FWidgetTextStyleData>& InStyles) { TextStyles = InStyles; }

	void SetBrushStyles(const TMap<FGameplayTag, FWidgetBrushStyleData>& InStyles) { BrushStyles = InStyles; }

	void SetSettingRendererClasses(const FSettingRendererClasses& InClasses) { SettingRendererClasses = InClasses; }
#endif
};
