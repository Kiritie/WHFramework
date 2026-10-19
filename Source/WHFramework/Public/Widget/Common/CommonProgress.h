#pragma once

#include "Widget/Common/CommonWidget.h"
#include "CommonProgress.generated.h"

class UProgressBar;
class UImage;
class UMaterialInstanceDynamic;

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonProgress : public UCommonWidget
{
	GENERATED_BODY()

public:
	UCommonProgress(const FObjectInitializer& ObjectInitializer);
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

	UFUNCTION(BlueprintCallable)
	void SetPercent(float InPercent);

	UFUNCTION(BlueprintPure)
	float GetPercent() const;

protected:
	virtual EWidgetStyleKind GetStyleKind() const override;
	virtual void BuildNativeLayout() override;
	virtual void ApplyResolvedStyle(const UObject* InStyle) override;
	virtual void SynchronizeProperties() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Radial;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Background;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float Percent = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RadialMaterial;

	FName ProgressParameter = TEXT("Progress");
};
