// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Components/TextWidgetTypes.h"
#include "Blueprint/UserWidget.h"
#include "Debug/DebugModuleTypes.h"
#include "Global/GlobalBPLibrary.h"
#include "Widget/Interfaces/TickAbleWidgetInterface.h"

#include "AnimTextBlock.generated.h"

class UDBTweenWidget;
class UTextBlock;
class STextBlock;

UENUM( BlueprintType )
enum class ETextHorizontalJustify : uint8
{
	Left,
	Center,
	Right
};

UENUM( BlueprintType )
enum class ETextVerticalJustify : uint8
{
	Top,
	Center,
	Bottom
};

UENUM( BlueprintType )
enum class ETextAnimType : uint8
{
	None,
	LerpOnly,
	LerpAndMove
};

UENUM( BlueprintType )
enum class ETextAnimDirection : uint8
{
	None,
	Up,
	Down
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAnimTextItem
{
	GENERATED_USTRUCT_BODY()

public:
	FAnimTextItem()
	{
		TargetText = FText::GetEmpty();
		TextBlock = nullptr;
		AnimTime = 0.f;
		CurrentText = FText::GetEmpty();
		AnimDirection = ETextAnimDirection::None;
	}
	
public:
	FText CurrentText;

	FText TargetText;

	UPROPERTY()
	UTextBlock* TextBlock;

	float AnimTime;
	
	FTimerHandle AnimTimerHandle;

	ETextAnimDirection AnimDirection;

public:
	bool IsNumber() const
	{
		return UGlobalBPLibrary::TextIsNumber(TargetText);
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAnimTextInfo
{
	GENERATED_USTRUCT_BODY()

public:
	FAnimTextInfo()
	{
		TargetText = FText::GetEmpty();
		CurrentNum = 0.f;
		TargetNum = 0.f;
		AnimSpeed = 0.f;
		TextItems = TArray<FAnimTextItem>();
		SymbolInfos = TMap<int32, FString>();
	}
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText TargetText;

	float CurrentNum;

	float TargetNum;

	float AnimSpeed;

	TArray<FAnimTextItem> TextItems;

	TMap<int32, FString> SymbolInfos;

public:
	FText GetCurrentText() const
	{
		return UGlobalBPLibrary::NumberToText(TargetNum - CurrentNum > 0.f ? FMath::FloorToInt(CurrentNum) : FMath::CeilToInt(CurrentNum), SymbolInfos);
	}

	void SetTargetText(FText InTargetText)
	{
		TargetText = InTargetText;
		TargetNum = UGlobalBPLibrary::TextToNumber(TargetText, SymbolInfos);
		AnimSpeed = FMath::Abs(TargetNum - CurrentNum);
	}
};

UCLASS()
class WHFRAMEWORK_API UAnimTextBlock : public UUserWidget, public ITickAbleWidgetInterface
{
	GENERATED_UCLASS_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category=Appearance, meta=( MultiLine="true" ))
	FText Text;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	ETextAnimType AnimType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "AnimType != ETextAnimType::None"), Category=Appearance)
	float AnimTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateFontInfo Font;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FSlateBrush StrikeBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FVector2D ShadowOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( DisplayName="Shadow Color" ))
	FLinearColor ShadowColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(DisplayName="Transform Policy"))
	ETextTransformPolicy TextTransformPolicy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(DisplayName="Horizontal Justify"))
	ETextHorizontalJustify TextHorizontalJustify;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(DisplayName="Vertical Justify"))
	ETextVerticalJustify TextVerticalJustify;

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UOverlay* TextOverlay;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UHorizontalBox* TextBox;
	
	FAnimTextInfo AnimTextInfo;

public:
	virtual void NativePreConstruct() override;

	virtual void NativeConstruct() override;

public:
	virtual bool IsTickAble_Implementation() const override;

	virtual void OnTick_Implementation(float DeltaSeconds) override;

protected:
	UFUNCTION()
	void PlayTextAnim(int32 TextIndex);

	void UpdatePreviewText();

	void ClearPreviewText();

public:
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Text)"))
	FText GetText() const;

	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Text)"))
	void SetText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetAnimType(ETextAnimType InAnimType);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetAnimTime(float InAnimTime);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetColor(FLinearColor InColor);
	
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetFont(FSlateFontInfo InFontInfo);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetOpacity(float InOpacity);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetShadowColor(FLinearColor InShadowColor);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetShadowOffset(FVector2D InShadowOffset);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetStrikeBrush(FSlateBrush InStrikeBrush);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetTextTransformPolicy(ETextTransformPolicy InTransformPolicy);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetTextHorizontalJustify(ETextHorizontalJustify InTextHorizontalJustify);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetTextVerticalJustify(ETextVerticalJustify InTextVerticalJustify);
};
