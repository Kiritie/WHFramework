// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/Widget/WidgetAchievementHUD.h"

#include "Achievement/AchievementModule.h"
#include "Achievement/Widget/WidgetAchievement.h"
#include "Common/CommonStatics.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

UWidgetAchievementHUD::UWidgetAchievementHUD(const FObjectInitializer &ObjectInitializer) : UUserWidgetBase(ObjectInitializer)
{
	WidgetName = FName("AchievementHUD");
	WidgetType = EWidgetType::Permanent;
	WidgetInputMode = EInputMode::None;

	WidgetZOrder = 5;

	WidgetCreateType = EWidgetCreateType::AutoCreateAndOpen;
	WidgetRefreshType = EWidgetRefreshType::Tick;
}

void UWidgetAchievementHUD::OnCreate(UObject* InOwner)
{
	Super::OnCreate(InOwner);
}

void UWidgetAchievementHUD::OnInitialize(UObject* InOwner)
{
	Super::OnInitialize(InOwner);
}

void UWidgetAchievementHUD::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);

	SpawnAnchors = FAnchors(
		UAchievementModule::Get()->IsRightOfScreen() ? 1 : 0,
		UAchievementModule::Get()->IsBottomOfScreen() ? 1 : 0
	);

	if(UAchievementModule::Get()->UnlockSound.IsValid())
	{
		UnlockSound_Loaded = Cast<USoundBase>(UAchievementModule::Get()->UnlockSound.TryLoad());
	}
	if(UAchievementModule::Get()->ProgressMadeSound.IsValid())
	{
		ProgressMadeSound_Loaded = Cast<USoundBase>(UAchievementModule::Get()->ProgressMadeSound.TryLoad());
	}
}

void UWidgetAchievementHUD::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

void UWidgetAchievementHUD::OnRefresh()
{
	Super::OnRefresh();

	if (Active.Num() < UAchievementModule::Get()->TotalOnScreen && !MovingUp)
	{
		if (BackLog.Num() > 0)
		{
			Spawn(BackLog[0].Data, BackLog[0].State);
			BackLog.RemoveAt(0);
		}
	}
	else
	{
		if (MovingUp)
		{
			CurrentTime += UCommonStatics::GetDeltaSeconds();
			for (UWidgetAchievement* i : Active)
			{
				UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(i->Slot);
				CSlot->SetPosition(FVector2D(
					CSlot->GetPosition().X,
					UKismetMathLibrary::MapRangeClamped(CurrentTime, 0, UAchievementModule::Get()->EntranceAnimationLength, i->StartHeight, i->StartHeight + (UAchievementModule::Get()->IsBottomOfScreen() ? -Size.Y : Size.Y))
				));
			}
			if (CurrentTime >= UAchievementModule::Get()->EntranceAnimationLength)
			{
				MovingUp = false;
				CurrentTime = 0;
				for (int i = 0; i < Active.Num(); i++)
				{
					Active[i]->StartHeight = Cast<UCanvasPanelSlot>(Active[i]->Slot)->GetPosition().Y;
				}
			}
		}
	}
}

void UWidgetAchievementHUD::OnDestroy(bool bRecovery)
{
	Super::OnDestroy(bRecovery);
}

void UWidgetAchievementHUD::ScheduleAchievementDisplay(FAchievementData Achievement, FAchievementStates State)
{
	if(Active.Num() < UAchievementModule::Get()->TotalOnScreen && !MovingUp)
	{
		Spawn(Achievement, State);
	}
	else
	{
		FAchievementPackage TempPackage;
		TempPackage.Data = Achievement;
		TempPackage.State = State;
		BackLog.Add(TempPackage);
	}
}

void UWidgetAchievementHUD::Spawn(FAchievementData Achievement, FAchievementStates State)
{
	if(!IsInViewport())
	{
		AddToViewport();
	}
	
	//USoundBase * Sound = Cast<USoundBase>((State.Achieved ? UAchievementModule::Get()->UnlockSound : UAchievementModule::Get()->ProgressMadeSound).TryLoad());
	//if(IsValid(Sound))
	//{
	//	UGameplayStatics::PlaySound2D(GetWorld(), Cast<USoundBase>(Sound));
	//}
	
	USoundBase * Sound = State.Achieved ? UnlockSound_Loaded : ProgressMadeSound_Loaded;
	if(IsValid(Sound))
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound);
	}
	
	UWidgetAchievement *Temp = CreateWidget<UWidgetAchievement>(UGameplayStatics::GetPlayerController(GetWorld(), 0), UAchievementModule::Get()->AchievementWidgetClass);
	Temp->SetValue(Achievement, State, UAchievementModule::Get()->bShowExactProgress);
	Active.Add(Temp);
	UCanvasPanelSlot *CSlot = Canvas->AddChildToCanvas(Temp);
	
	Size = FVector2D(Temp->Root->GetWidthOverride(), Temp->Root->GetHeightOverride());
	SpawnLocation = FVector2D(
		UAchievementModule::Get()->IsRightOfScreen() ? -Size.X : 0,
		UAchievementModule::Get()->IsBottomOfScreen() ? 0 : -Size.Y
	);
	
	CSlot->SetSize(Size);
	CSlot->SetAnchors(SpawnAnchors);
	CSlot->SetPosition(SpawnLocation);
	Temp->OnDeath.AddDynamic(this, &UWidgetAchievementHUD::OnAchievementDeath);
	Temp->StartDeathTimer(UAchievementModule::Get()->ExitAnimationLength, UAchievementModule::Get()->ScreenTime, UAchievementModule::Get()->ExitAnimation, UAchievementModule::Get()->IsRightOfScreen());
	Temp->StartHeight = SpawnLocation.Y;
	MovingUp = true;
}

void UWidgetAchievementHUD::OnAchievementDeath(UWidgetAchievement* Achievement)
{
	Achievement->OnDeath.RemoveAll(this);
	Active.Remove(Achievement);
	Achievement->RemoveFromParent();
}