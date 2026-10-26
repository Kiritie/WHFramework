#pragma once

#include "Widget/Common/CommonWidget.h"
#include "CommonWindow.generated.h"

class UBorder;
class UNamedSlot;
class UCommonTextBlockN;
class UCommonButton;
class UHorizontalBox;
class USizeBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCommonWindowCloseRequested);

/** 基础窗体与标题栏，内容由 NamedSlot 承载 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UCommonWindow : public UCommonWidget
{
	GENERATED_BODY()

public:
	UCommonWindow(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable)
	void SetTitle(const FText& InTitle);

	UFUNCTION(BlueprintCallable)
	void SetShowCloseButton(bool bInShowCloseButton);

	UFUNCTION(BlueprintCallable)
	void SetCloseButtonStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable)
	void RequestClose();

	UPROPERTY(BlueprintAssignable)
	FCommonWindowCloseRequested OnCloseRequested;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

	virtual void GetSlotNames(TArray<FName>& SlotNames) const override;
	virtual UWidget* GetContentForSlot(FName SlotName) const override;
	virtual void SetContentForSlot(FName SlotName, UWidget* InContent) override;
	virtual void SynchronizeProperties() override;

protected:
	virtual void InitializeNativeClassData() override;
	virtual EWidgetStyleKind GetStyleKind() const override;
	virtual void BuildNativeLayout() override;
	virtual void ApplyResolvedStyle(const UObject* InStyle) override;
	virtual void BindControlEvents() override;
	virtual void UnbindControlEvents() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bShowCloseButton = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "UI.Style.Button"))
	FGameplayTag CloseButtonStyleTag;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> TitleBar;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> TitleBarSize;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> TitleSeparatorSize;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TitleSeparator;

	UPROPERTY(Transient)
	TObjectPtr<UCommonButton> Btn_Close;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowBody;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlockN> Txt_Title;

	UPROPERTY(Transient)
	TObjectPtr<UNamedSlot> Content;
};
