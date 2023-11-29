// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/Base/WHObject.h"
#include "Math/MathTypes.h"
#include "WidgetAnimatorBase.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnWidgetAnimatorCompleted, bool, bInstant);

class UWidget;
class UUserWidget;

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UWidgetAnimatorBase : public UWHObject
{
	GENERATED_BODY()

public:
	UWidgetAnimatorBase();

public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, meta = (AutoCreateRefTerm = "OnCompleted"))
	void Play(const FOnWidgetAnimatorCompleted& OnCompleted, bool bInstant = false);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool Abort();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Pause();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Reset();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Stop();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Complete(bool bInstant = false);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animator")
	FName WidgetName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animator")
	FGameplayTag SelfTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animator")
	FGameplayTagContainer AbortTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animator")
	float Duration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animator")
	EEaseType EaseType;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Animator")
	UWidget* TargetWidget;

	UPROPERTY(BlueprintReadOnly, Category = "Animator")
	UUserWidget* ParentWidget;

private:
	FOnWidgetAnimatorCompleted _OnCompleted;
};
