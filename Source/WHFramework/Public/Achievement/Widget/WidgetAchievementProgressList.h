// Copyright 2021 Sam Carey. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "WidgetAchievementProgressList.generated.h"

UENUM(BlueprintType)
enum class EAchievementProgressFilter : uint8
{
	ANY UMETA(DisplayName = "Show All"),
	ACHIEVED UMETA(DisplayName = "Achieved Only"),
	UNACHIEVED UMETA(DisplayName = "Unachieved Only"),
};

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetAchievementProgressList : public UUserWidget
{
	GENERATED_BODY()

public:
	UWidgetAchievementProgressList(const FObjectInitializer& ObjectInitializer);

	/**Change the type of achievements to show. Must all UpdateList() after changing is done at runtime*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Details")
	EAchievementProgressFilter Filter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Details")
	bool bShowHiddenAchievementCount = false;

	UFUNCTION(BlueprintCallable, Category = "Achievement Progress List")
	void UpdateList();

protected:
	virtual void NativePreConstruct() override;

	virtual void NativeConstruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	UScrollBox* ListView;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* HiddenText;
};
