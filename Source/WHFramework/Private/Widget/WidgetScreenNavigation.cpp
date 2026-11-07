#include "Widget/WidgetMountTypes.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Widget/ScreenWidgetTypes.h"
#include "Widget/WidgetModule.h"

#include "WHFrameworkCoreStatics.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Common/CommonModuleStatics.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Widget/Screen/WidgetMountSlot.h"
#include "Widget/WidgetParameterHelpers.h"

using namespace WidgetParameterHelpers;

bool UWidgetModule::MountUserWidget(APlayerController* InPlayer, UUserWidgetBase* InWidget, const FScreenWidgetConfig& InConfig)
{
	if(!InWidget)
	{
		return false;
	}

	const FGameplayTag WidgetTag = InConfig.ResolveWidgetTag();
	const FGameplayTag ParentWidgetTag = InConfig.ResolveParentWidgetTag();
	if(!ParentWidgetTag.IsValid())
	{
		return InWidget->AddToPlayerScreen(InConfig.ZOrder);
	}

	UUserWidgetBase* ParentWidget = GetUserWidgetByTag(InPlayer, ParentWidgetTag);
	if(!ParentWidget)
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Failed to mount widget %s: parent %s does not exist."), *WidgetTag.ToString(), *ParentWidgetTag.ToString()), EDC_Widget, EDV_Error);
		return false;
	}

	UPanelWidget* MountPanel = nullptr;
	if(InConfig.SlotTag.IsValid())
	{
		MountPanel = ParentWidget->GetWidgetMountSlot(InConfig.SlotTag);
		if(!MountPanel)
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to mount widget %s: slot %s was not found in parent %s."), *WidgetTag.ToString(), *InConfig.SlotTag.ToString(), *ParentWidgetTag.ToString()), EDC_Widget, EDV_Error);
			return false;
		}
	}
	else
	{
		MountPanel = Cast<UPanelWidget>(ParentWidget->GetRootWidget());
		if(!MountPanel)
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to mount widget %s: parent %s root widget is not a PanelWidget."), *WidgetTag.ToString(), *ParentWidgetTag.ToString()), EDC_Widget, EDV_Error);
			return false;
		}
	}

	if(UContentWidget* ContentWidget = Cast<UContentWidget>(MountPanel))
	{
		if(ContentWidget->GetContent() && ContentWidget->GetContent() != InWidget)
		{
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to mount widget %s: mount target in parent %s already contains %s."), *WidgetTag.ToString(), *ParentWidgetTag.ToString(), *GetNameSafe(ContentWidget->GetContent())), EDC_Widget, EDV_Error);
			return false;
		}
		ContentWidget->SetContent(InWidget);
	}
	else if(UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(MountPanel))
	{
		UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(InWidget);
		if(!CanvasSlot)
		{
			return false;
		}
		CanvasSlot->SetZOrder(InConfig.ZOrder);
		CanvasSlot->SetAnchors(InConfig.Anchors);
		CanvasSlot->SetOffsets(InConfig.Offsets);
		CanvasSlot->SetAlignment(InConfig.Alignment);
	}
	else if(!MountPanel->AddChild(InWidget))
	{
		ensureEditorMsgf(false, FString::Printf(TEXT("Failed to add widget %s to parent %s panel %s."), *WidgetTag.ToString(), *ParentWidgetTag.ToString(), *GetNameSafe(MountPanel)), EDC_Widget, EDV_Error);
		return false;
	}

	InWidget->ParentWidget = ParentWidget;
	ParentWidget->AddChildWidget(InWidget);
	return true;
}

void UWidgetModule::UnMountUserWidget(UUserWidgetBase* InWidget)
{
	if(InWidget->IsInViewport() || InWidget->GetParent())
	{
		InWidget->RemoveFromParent();
	}
	if(InWidget->ParentWidget)
	{
		InWidget->ParentWidget->RemoveChildWidget(InWidget);
		InWidget->ParentWidget = nullptr;
	}
}

UUserWidgetBase* UWidgetModule::CreateUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, const FParameter& InParam, TSubclassOf<UUserWidgetBase> InClass)
{
	if (!IsBusinessAdmissionOpen()) return nullptr;
	if (!bPlayerWidgetsReady || !ResolveLocalPlayer(InPlayer)) return nullptr;
	if(UUserWidgetBase* Existing = GetUserWidgetByTag(InPlayer, InWidgetTag))
	{
		return Existing;
	}

	FParameter SpawnParam = MakeWidgetSpawnParam(InParam);
	FWidgetSpawnParameter* Spawn = SpawnParam.GetMutablePtr<FWidgetSpawnParameter>();
	if (!Spawn) return nullptr;
	Spawn->OwningPlayer = InPlayer;
	const FParameter OwnerParam = MakeWidgetSpawnOwnerParam(SpawnParam);
	const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag);
	if(!Config)
	{
		return nullptr;
	}
	const FScreenWidgetConfig ConfigSnapshot = *Config;
	const FGameplayTag ParentWidgetTag = Config->ResolveParentWidgetTag();
	if(ParentWidgetTag.IsValid() && !GetUserWidgetByTag(InPlayer, ParentWidgetTag))
	{
		return nullptr;
	}

	const TSubclassOf<UUserWidgetBase> SpawnClass = InClass ? InClass : Config->WidgetClass;
	UObjectPoolModule* Pool = UObjectPoolModule::GetPtr(GetWorld());
	UUserWidgetBase* Widget = SpawnClass && Pool
		? Cast<UUserWidgetBase>(Pool->SpawnObject(SpawnClass.Get(), SpawnParam))
		: nullptr;
	if(!Widget)
	{
		return nullptr;
	}

	Widget->WidgetTag = ConfigSnapshot.ResolveWidgetTag();
	FScreenWidgetKey Key; Key.LocalPlayer = ResolveLocalPlayer(InPlayer); Key.WidgetTag = InWidgetTag;
	if (!Key.LocalPlayer) return nullptr;
	UserWidgetByTag.Add(Key, Widget);

	Widget->OnCreate(SpawnParam);
	Widget->Init(SpawnParam, false);
	Widget->SetVisibility(ESlateVisibility::Hidden);
	const bool bMounted = MountUserWidget(InPlayer, Widget, ConfigSnapshot);
	if(!bMounted)
	{
		UserWidgetByTag.Remove(Key);
		Widget->OnDestroy(EObjectDespawnMode::Destroy);
		return nullptr;
	}
	TArray<FGameplayTag> ChildTags;
	UserWidgetChildrenMap.MultiFind(InWidgetTag, ChildTags);
	for(const FGameplayTag& ChildTag : ChildTags)
	{
		const FScreenWidgetConfig* ChildConfig = GetUserWidgetConfig(ChildTag);
		if(!ChildConfig)
		{
			continue;
		}
		if(ChildConfig->CreateType == EWidgetCreateType::AutoCreate
			|| ChildConfig->CreateType == EWidgetCreateType::AutoCreateAndOpen)
		{
			CreateUserWidgetByTag(InPlayer, ChildTag, OwnerParam);
		}
	}
	return Widget;
}

bool UWidgetModule::OpenUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, const FParameter& InParam, bool bInstant, bool bForce, TSubclassOf<UUserWidgetBase> InClass)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!bPlayerWidgetsReady || !ResolveLocalPlayer(InPlayer)) return false;
	const FParameter OpenParam = MakeWidgetOpenParam(InParam);
	const FParameter SpawnOwnerParam = MakeWidgetSpawnOwnerParam(OpenParam);
	const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag);
	if(!Config)
	{
		return false;
	}
	const FScreenWidgetConfig ConfigSnapshot = *Config;
	const FGameplayTag ParentWidgetTag = Config->ResolveParentWidgetTag();
	if(ParentWidgetTag.IsValid())
	{
		const UUserWidgetBase* ParentWidget = GetUserWidgetByTag(InPlayer, ParentWidgetTag);
		if(!ParentWidget || !ParentWidget->IsWidgetOpened())
		{
			return false;
		}
	}

	UUserWidgetBase* Widget = GetUserWidgetByTag(InPlayer, InWidgetTag);
	if(!Widget)
	{
		Widget = CreateUserWidgetByTag(
			InPlayer, InWidgetTag,
			SpawnOwnerParam,
			InClass);
	}
	if(!Widget || !Widget->CanOpen())
	{
		return false;
	}

	if(ConfigSnapshot.WidgetType == EWidgetType::Temporary)
	{
		FWidgetMountContext Context;
		Context.LocalPlayer = ResolveLocalPlayer(InPlayer);
		if (!Context.LocalPlayer) return false;
		Context.ParentWidgetTag = ParentWidgetTag;
		Context.SlotTag = ConfigSnapshot.SlotTag;
		if(UUserWidgetBase* ActiveWidget = ActiveTemporaryWidgets.FindRef(Context); ActiveWidget && ActiveWidget != Widget)
		{
			FTemporaryWidgetHistory& History = TemporaryWidgetHistories.FindOrAdd(Context);
			History.Widgets.Remove(Widget);
			History.Widgets.Add(ActiveWidget);
			ActiveWidget->OnClosed.RemoveAll(this);
			ActiveWidget->OnClose(true);
		}
		ActiveTemporaryWidgets.Add(Context, Widget);
	}

	if(bForce && Widget->GetWidgetState() != EScreenWidgetState::None)
	{
		Widget->OnClose(true);
	}
	const EScreenWidgetState PreviousState = Widget->GetWidgetState();
	Widget->OnOpen(OpenParam, bInstant);
	if(PreviousState != EScreenWidgetState::Opening && PreviousState != EScreenWidgetState::Opened)
	{
		TArray<FGameplayTag> ChildTags;
		UserWidgetChildrenMap.MultiFind(InWidgetTag, ChildTags);
		for(const FGameplayTag& ChildTag : ChildTags)
		{
			const FScreenWidgetConfig* ChildConfig = GetUserWidgetConfig(ChildTag);
			if(ChildConfig
				&& ChildConfig->CreateType == EWidgetCreateType::AutoCreateAndOpen)
			{
				OpenUserWidgetByTag(InPlayer, ChildTag, FWidgetOpenParameter(), bInstant);
			}
		}
	}
	return true;
}

bool UWidgetModule::CloseUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, bool bInstant)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if(UUserWidgetBase* Widget = GetUserWidgetByTag(InPlayer, InWidgetTag))
	{
		if(const FScreenWidgetConfig* Config = GetUserWidgetConfig(InWidgetTag); Config && Config->WidgetType == EWidgetType::Temporary)
		{
			FWidgetMountContext Context;
			Context.LocalPlayer = ResolveLocalPlayer(InPlayer);
			Context.ParentWidgetTag = Config->ResolveParentWidgetTag();
			Context.SlotTag = Config->SlotTag;
			if(ActiveTemporaryWidgets.FindRef(Context) == Widget)
			{
				ActiveTemporaryWidgets.Remove(Context);
				Widget->OnClosed.RemoveAll(this);
				Widget->OnClosed.AddUObject(this, &ThisClass::HandleTemporaryWidgetClosed, Context, Widget);
			}
		}
		Widget->OnClose(bInstant);
		return true;
	}
	return false;
}

bool UWidgetModule::ToggleUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, bool bInstant)
{
	if (!IsBusinessAdmissionOpen()) return false;
	UUserWidgetBase* Widget = GetUserWidgetByTag(InPlayer, InWidgetTag);
	if(!Widget)
	{
		return OpenUserWidgetByTag(InPlayer, InWidgetTag, FWidgetOpenParameter(), bInstant);
	}
	if(Widget->GetWidgetState() == EScreenWidgetState::Opened)
	{
		return CloseUserWidgetByTag(InPlayer, InWidgetTag, bInstant);
	}
	return OpenUserWidgetByTag(InPlayer, InWidgetTag, FWidgetOpenParameter(), bInstant);
}

bool UWidgetModule::CloseActiveTemporaryWidget(APlayerController* InPlayer, const FWidgetMountContext& InContext, bool bInstant)
{
	if (!IsBusinessAdmissionOpen()) return false;
	ULocalPlayer* LocalPlayer = ResolveLocalPlayer(InPlayer);
	if (!LocalPlayer) return false;
	FWidgetMountContext Context = InContext;
	Context.LocalPlayer = LocalPlayer;
	if(UUserWidgetBase* Widget = ActiveTemporaryWidgets.FindRef(Context))
	{
		return CloseUserWidgetByTag(InPlayer, Widget->GetWidgetTag(), bInstant);
	}
	return false;
}

void UWidgetModule::HandleTemporaryWidgetClosed(bool bInstant, FWidgetMountContext InContext, UUserWidgetBase* InWidget)
{
	if (!IsBusinessAdmissionOpen()) return;
	InWidget->OnClosed.RemoveAll(this);
	APlayerController* InPlayer = InContext.LocalPlayer ? InContext.LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	if (!ResolveLocalPlayer(InPlayer) || ActiveTemporaryWidgets.Contains(InContext))
	{
		return;
	}

	while (FTemporaryWidgetHistory* History = TemporaryWidgetHistories.Find(InContext))
	{
		if (History->Widgets.IsEmpty())
		{
			TemporaryWidgetHistories.Remove(InContext);
			break;
		}
		UUserWidgetBase* PreviousWidget = History->Widgets.Pop();
		if(PreviousWidget && OpenUserWidgetByTag(
			InPlayer, PreviousWidget->GetWidgetTag(),
			PreviousWidget->CurrentOpenParameter,
			bInstant))
		{
			break;
		}
	}
	if (const FTemporaryWidgetHistory* History = TemporaryWidgetHistories.Find(InContext); History && History->Widgets.IsEmpty())
	{
		TemporaryWidgetHistories.Remove(InContext);
	}
}

bool UWidgetModule::CloseTemporaryWidgetInSlot(APlayerController* InPlayer, FGameplayTag InParentWidgetTag, FGameplayTag InSlotTag, bool bInstant)
{
	if (!IsBusinessAdmissionOpen()) return false;
	FWidgetMountContext Context;
	Context.LocalPlayer = ResolveLocalPlayer(InPlayer);
	if (!Context.LocalPlayer) return false;
	Context.ParentWidgetTag = InParentWidgetTag;
	Context.SlotTag = InSlotTag;
	return CloseActiveTemporaryWidget(InPlayer, Context, bInstant);
}

void UWidgetModule::CloseTemporaryWidgetsForParent(APlayerController* InPlayer, FGameplayTag InParentWidgetTag, bool bInstant)
{
	if (!IsBusinessAdmissionOpen()) return;
	ULocalPlayer* LocalPlayer = ResolveLocalPlayer(InPlayer);
	if (!LocalPlayer) return;
	TArray<FWidgetMountContext> Contexts;
	ActiveTemporaryWidgets.GetKeys(Contexts);
	for(const FWidgetMountContext& Context : Contexts)
	{
		if(Context.LocalPlayer == LocalPlayer && Context.ParentWidgetTag == InParentWidgetTag)
		{
			TemporaryWidgetHistories.Remove(Context);
			CloseActiveTemporaryWidget(InPlayer, Context, bInstant);
		}
	}
}

bool UWidgetModule::DestroyUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, EObjectDespawnMode InMode)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if(UUserWidgetBase* Widget = GetUserWidgetByTag(InPlayer, InWidgetTag))
	{
		FScreenWidgetKey Key; Key.LocalPlayer = ResolveLocalPlayer(InPlayer); Key.WidgetTag = InWidgetTag;
		UserWidgetByTag.Remove(Key);

		TArray<FGameplayTag> ChildTags;
		UserWidgetChildrenMap.MultiFind(InWidgetTag, ChildTags);
		for(const FGameplayTag& ChildTag : ChildTags)
		{
			DestroyUserWidgetByTag(InPlayer, ChildTag, InMode);
		}

		TArray<FWidgetMountContext> TemporaryContexts;
		ActiveTemporaryWidgets.GetKeys(TemporaryContexts);
		for(const FWidgetMountContext& Context : TemporaryContexts)
		{
			if(Context.LocalPlayer == ResolveLocalPlayer(InPlayer) &&
                (Context.ParentWidgetTag == InWidgetTag || ActiveTemporaryWidgets.FindRef(Context) == Widget))
			{
				ActiveTemporaryWidgets.Remove(Context);
				TemporaryWidgetHistories.Remove(Context);
			}
		}
		TArray<FWidgetMountContext> HistoryContexts;
		TemporaryWidgetHistories.GetKeys(HistoryContexts);
		for(const FWidgetMountContext& Context : HistoryContexts)
		{
			if(Context.LocalPlayer != ResolveLocalPlayer(InPlayer)) continue;
			if(Context.ParentWidgetTag == InWidgetTag)
			{
				TemporaryWidgetHistories.Remove(Context);
			}
			else if(FTemporaryWidgetHistory* History = TemporaryWidgetHistories.Find(Context))
			{
				History->Widgets.Remove(Widget);
				if(History->Widgets.IsEmpty())
				{
					TemporaryWidgetHistories.Remove(Context);
				}
			}
		}

		UnMountUserWidget(Widget);
		Widget->OnDestroy(InMode);

		return true;
	}
	return false;
}
