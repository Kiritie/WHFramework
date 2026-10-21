// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Widget/WidgetWorldText.h"

#include "Animation/WidgetAnimation.h"
#include "Common/CommonModuleTypes.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Scene/SceneModuleTypes.h"
#include "TimerManager.h"

UWidgetWorldText::UWidgetWorldText(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetWorldText::OnCreate(FWorldWidgetMapping InMapping, const FParameter& InParam)
{
	const FScopedWorldContext WorldContext(GetWorld());
	const uint64 Session = ++TextSession;
	GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	StopAllAnimations();
	if (Session != TextSession) return;
	Super::OnCreate(InMapping, InParam);
	if (Session != TextSession) return;
	if (GetVisibility() != ESlateVisibility::SelfHitTestInvisible)
	{
		Destroy();
		return;
	}

	const FWorldTextWidgetParameter DefaultParameter;
	const FWorldTextWidgetParameter* Parameter = InParam.GetPtr<FWorldTextWidgetParameter>();
	if (!Parameter) Parameter = &DefaultParameter;
	UWidgetAnimation* Animation = nullptr;
	if (Parameter->TextStyle == static_cast<int32>(EWorldTextStyle::Normal)) Animation = NormalAnim;
	else if (Parameter->TextStyle == static_cast<int32>(EWorldTextStyle::Stress)) Animation = StressAnim;
	const float Duration = Animation ? Animation->GetEndTime() - Animation->GetStartTime() : 0.f;
	if (!FMath::IsFinite(Duration) || Duration <= 0.f)
	{
		Destroy();
		return;
	}
	TextContent->SetText(FText::FromString(Parameter->Text));
	TextContent->SetColorAndOpacity(FSlateColor(Parameter->TextColor));
	GetWorld()->GetTimerManager().SetTimer(ExpiryTimer, FTimerDelegate::CreateWeakLambda(this, [this, Session]()
	{
		if (Session != TextSession) return;
		const FScopedWorldContext CallbackWorldContext(GetWorld());
		Destroy();
	}), Duration, false);
	PlayAnimationForward(Animation);
}

void UWidgetWorldText::OnDestroy(EObjectDespawnMode InMode)
{
	const FScopedWorldContext WorldContext(GetWorld());
	++TextSession;
	GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	StopAllAnimations();
	Super::OnDestroy(InMode);
}
