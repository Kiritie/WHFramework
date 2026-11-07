// Copyright 2021 Sam Carey. All Rights Reserved.

#include "Achievement/Widget/WidgetAchievementHUD.h"

#include "Achievement/AchievementModule.h"
#include "Achievement/Widget/WidgetAchievement.h"
#include "Common/CommonModuleStatics.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

UWidgetAchievementHUD::UWidgetAchievementHUD(const FObjectInitializer &ObjectInitializer) : UUserWidgetBase(ObjectInitializer)
{
	WidgetRefreshType = EWidgetRefreshType::Tick;
	
}

void UWidgetAchievementHUD::OnCreate(const FParameter& InParams)
{
	Super::OnCreate(InParams);
}

void UWidgetAchievementHUD::OnInitialize(const FParameter& InParams)
{
	Super::OnInitialize(InParams);
}

void UWidgetAchievementHUD::OnOpen(const FParameter& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
	const UAchievementModule* Module = UAchievementModule::GetPtr(GetWorld());
	if (!Module) return;

	SpawnAnchors = FAnchors(
		Module->IsRightOfScreen() ? 1 : 0,
		Module->IsBottomOfScreen() ? 1 : 0
	);

	if(Module->UnlockSound.IsValid())
	{
		UnlockSound_Loaded = Cast<USoundBase>(Module->UnlockSound.TryLoad());
	}
	if(Module->ProgressMadeSound.IsValid())
	{
		ProgressMadeSound_Loaded = Cast<USoundBase>(Module->ProgressMadeSound.TryLoad());
	}
}

void UWidgetAchievementHUD::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

void UWidgetAchievementHUD::OnRefresh()
{
	Super::OnRefresh();
	const UAchievementModule* Module = UAchievementModule::GetPtr(GetWorld());
	if (!Module) return;

	if (Active.Num() < Module->TotalOnScreen && !MovingUp)
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
			CurrentTime += GetWorld()->GetDeltaSeconds();
			for (UWidgetAchievement* i : Active)
			{
				UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(i->Slot);
				CSlot->SetPosition(FVector2D(
					CSlot->GetPosition().X,
					UKismetMathLibrary::MapRangeClamped(CurrentTime, 0, Module->EntranceAnimationLength, i->StartHeight, i->StartHeight + (Module->IsBottomOfScreen() ? -Size.Y : Size.Y))
				));
			}
			if (CurrentTime >= Module->EntranceAnimationLength)
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

void UWidgetAchievementHUD::OnDestroy(EObjectDespawnMode InMode)
{
	Super::OnDestroy(InMode);
}

void UWidgetAchievementHUD::ScheduleAchievementDisplay(FAchievementData Achievement, FAchievementStates State)
{
	const UAchievementModule* Module = UAchievementModule::GetPtr(GetWorld());
	if (!Module) return;
	if(Active.Num() < Module->TotalOnScreen && !MovingUp)
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
	const UAchievementModule* Module = UAchievementModule::GetPtr(GetWorld());
	APlayerController* Player = GetOwningPlayer();
	if (!Module || !Player) return;
	if(!IsInViewport())
	{
		AddToPlayerScreen();
	}
	
	USoundBase * Sound = State.Achieved ? UnlockSound_Loaded : ProgressMadeSound_Loaded;
	if(IsValid(Sound))
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound);
	}
	
	UWidgetAchievement *Temp = CreateWidget<UWidgetAchievement>(Player, Module->AchievementWidgetClass);
	if (!Temp) return;
	Temp->SetValue(Achievement, State, Module->bShowExactProgress);
	Active.Add(Temp);
	UCanvasPanelSlot *CSlot = Canvas->AddChildToCanvas(Temp);
	
	Size = FVector2D(Temp->Root->GetWidthOverride(), Temp->Root->GetHeightOverride());
	SpawnLocation = FVector2D(
		Module->IsRightOfScreen() ? -Size.X : 0,
		Module->IsBottomOfScreen() ? 0 : -Size.Y
	);
	
	CSlot->SetSize(Size);
	CSlot->SetAnchors(SpawnAnchors);
	CSlot->SetPosition(SpawnLocation);
	Temp->OnDeath.AddDynamic(this, &UWidgetAchievementHUD::OnAchievementDeath);
	Temp->StartDeathTimer(Module->ExitAnimationLength, Module->ScreenTime, Module->ExitAnimation, Module->IsRightOfScreen());
	Temp->StartHeight = SpawnLocation.Y;
	MovingUp = true;
}

void UWidgetAchievementHUD::OnAchievementDeath(UWidgetAchievement* Achievement)
{
	Achievement->OnDeath.RemoveAll(this);
	Active.Remove(Achievement);
	Achievement->RemoveFromParent();
}
