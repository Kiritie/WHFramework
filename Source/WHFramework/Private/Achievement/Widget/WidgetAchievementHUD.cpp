// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/Widget/WidgetAchievementHUD.h"

#include "Achievement/AchievementModule.h"
#include "Achievement/Widget/WidgetAchievement.h"
#include "Common/CommonBPLibrary.h"
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

void UWidgetAchievementHUD::OnCreate_Implementation(UObject* InOwner)
{
	Super::OnCreate_Implementation(InOwner);
}

void UWidgetAchievementHUD::OnInitialize_Implementation(UObject* InOwner)
{
	Super::OnInitialize_Implementation(InOwner);
}

void UWidgetAchievementHUD::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);

	SpawnAnchors = FAnchors(
		AAchievementModule::Get()->IsRightOfScreen() ? 1 : 0,
		AAchievementModule::Get()->IsBottomOfScreen() ? 1 : 0
	);

	if(AAchievementModule::Get()->UnlockSound.IsValid())
	{
		UnlockSound_Loaded = Cast<USoundBase>(AAchievementModule::Get()->UnlockSound.TryLoad());
	}
	if(AAchievementModule::Get()->ProgressMadeSound.IsValid())
	{
		ProgressMadeSound_Loaded = Cast<USoundBase>(AAchievementModule::Get()->ProgressMadeSound.TryLoad());
	}
}

void UWidgetAchievementHUD::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

void UWidgetAchievementHUD::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();

	if (Active.Num() < AAchievementModule::Get()->TotalOnScreen && !MovingUp)
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
			CurrentTime += UCommonBPLibrary::GetDeltaSeconds();
			for (UWidgetAchievement* i : Active)
			{
				UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(i->Slot);
				CSlot->SetPosition(FVector2D(
					CSlot->GetPosition().X,
					UKismetMathLibrary::MapRangeClamped(CurrentTime, 0, AAchievementModule::Get()->EntranceAnimationLength, i->StartHeight, i->StartHeight + (AAchievementModule::Get()->IsBottomOfScreen() ? -Size.Y : Size.Y))
				));
			}
			if (CurrentTime >= AAchievementModule::Get()->EntranceAnimationLength)
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

void UWidgetAchievementHUD::OnDestroy_Implementation(bool bRecovery)
{
	Super::OnDestroy_Implementation(bRecovery);
}

void UWidgetAchievementHUD::ScheduleAchievementDisplay(FAchievementData Achievement, FAchievementStates State)
{
	if(Active.Num() < AAchievementModule::Get()->TotalOnScreen && !MovingUp)
	{
		Spawn(Achievement, State);
	}
	else
	{
		FAchievementPackage TempPackage = FAchievementPackage();
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
	
	//USoundBase * Sound = Cast<USoundBase>((State.Achieved ? AAchievementModule::Get()->UnlockSound : AAchievementModule::Get()->ProgressMadeSound).TryLoad());
	//if(IsValid(Sound))
	//{
	//	UGameplayStatics::PlaySound2D(GetWorld(), Cast<USoundBase>(Sound));
	//}
	
	USoundBase * Sound = State.Achieved ? UnlockSound_Loaded : ProgressMadeSound_Loaded;
	if(IsValid(Sound))
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound);
	}
	
	UWidgetAchievement *Temp = CreateWidget<UWidgetAchievement>(UGameplayStatics::GetPlayerController(GetWorld(), 0), AAchievementModule::Get()->AchievementWidgetClass);
	Temp->SetValue(Achievement, State, AAchievementModule::Get()->bShowExactProgress);
	Active.Add(Temp);
	UCanvasPanelSlot *CSlot = Canvas->AddChildToCanvas(Temp);
	
	Size = FVector2D(Temp->Root->GetWidthOverride(), Temp->Root->GetHeightOverride());
	SpawnLocation = FVector2D(
		AAchievementModule::Get()->IsRightOfScreen() ? -Size.X : 0,
		AAchievementModule::Get()->IsBottomOfScreen() ? 0 : -Size.Y
	);
	
	CSlot->SetSize(Size);
	CSlot->SetAnchors(SpawnAnchors);
	CSlot->SetPosition(SpawnLocation);
	Temp->OnDeath.AddDynamic(this, &UWidgetAchievementHUD::OnAchievementDeath);
	Temp->StartDeathTimer(AAchievementModule::Get()->ExitAnimationLength, AAchievementModule::Get()->ScreenTime, AAchievementModule::Get()->ExitAnimation, AAchievementModule::Get()->IsRightOfScreen());
	Temp->StartHeight = SpawnLocation.Y;
	MovingUp = true;
}

void UWidgetAchievementHUD::OnAchievementDeath(UWidgetAchievement* Achievement)
{
	Achievement->OnDeath.RemoveAll(this);
	Active.Remove(Achievement);
	Achievement->RemoveFromParent();
}