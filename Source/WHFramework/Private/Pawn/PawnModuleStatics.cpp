#include "Pawn/PawnModuleStatics.h"

#include "GameFramework/PlayerController.h"
#include "Pawn/Base/PawnBase.h"
#include "Pawn/PawnModule.h"
#include "Pawn/PawnModuleNetworkComponent.h"

APawnBase* UPawnModuleStatics::GetCurrentPawn(APlayerController* Player, TSubclassOf<APawnBase> Class)
{
	const UPawnModule* Module = Player ? UPawnModule::GetPtr(Player->GetWorld()) : nullptr;
	return Module ? Module->GetCurrentPawn(Player, Class) : nullptr;
}

TArray<APawnBase*> UPawnModuleStatics::GetAllPawn(UWorld* World)
{
	const UPawnModule* Module = UPawnModule::GetPtr(World);
	return Module ? Module->GetAllPawn() : TArray<APawnBase*>();
}

bool UPawnModuleStatics::SwitchPawn(APlayerController* Player, APawnBase* Pawn, bool bResetCamera, bool bInstant)
{
	if (!Player || (Pawn && Pawn->GetWorld() != Player->GetWorld())) return false;
	if (Player->HasAuthority())
	{
		UPawnModule* Module = UPawnModule::GetPtr(Player->GetWorld());
		return Module && Module->SwitchPawn(Player, Pawn, bResetCamera, bInstant);
	}
	UPawnModuleNetworkComponent* Network = Player->IsLocalController() ? Player->FindComponentByClass<UPawnModuleNetworkComponent>() : nullptr;
	return Network && Network->RequestPawnSwitch(Pawn, bResetCamera, bInstant);
}

bool UPawnModuleStatics::SwitchPawnByClass(APlayerController* Player, TSubclassOf<APawnBase> Class, bool bResetCamera, bool bInstant)
{
	const UPawnModule* Module = Player ? UPawnModule::GetPtr(Player->GetWorld()) : nullptr;
	APawnBase* Pawn = Module ? Module->GetPawnByClass(Class) : nullptr;
	return Pawn && SwitchPawn(Player, Pawn, bResetCamera, bInstant);
}

bool UPawnModuleStatics::SwitchPawnByName(APlayerController* Player, FName Name, bool bResetCamera, bool bInstant)
{
	const UPawnModule* Module = Player ? UPawnModule::GetPtr(Player->GetWorld()) : nullptr;
	APawnBase* Pawn = Module ? Module->GetPawnByName(Name) : nullptr;
	return Pawn && SwitchPawn(Player, Pawn, bResetCamera, bInstant);
}

bool UPawnModuleStatics::HasPawnByClass(UWorld* World, TSubclassOf<APawnBase> Class)
{
	const UPawnModule* Module = UPawnModule::GetPtr(World);
	return Module && Module->HasPawnByClass(Class);
}

bool UPawnModuleStatics::HasPawnByName(UWorld* World, FName Name)
{
	const UPawnModule* Module = UPawnModule::GetPtr(World);
	return Module && Module->HasPawnByName(Name);
}

APawnBase* UPawnModuleStatics::GetPawnByClass(UWorld* World, TSubclassOf<APawnBase> Class)
{
	const UPawnModule* Module = UPawnModule::GetPtr(World);
	return Module ? Module->GetPawnByClass(Class) : nullptr;
}

APawnBase* UPawnModuleStatics::GetPawnByName(UWorld* World, FName Name)
{
	const UPawnModule* Module = UPawnModule::GetPtr(World);
	return Module ? Module->GetPawnByName(Name) : nullptr;
}

void UPawnModuleStatics::AddPawnToList(APawnBase* Pawn)
{
	if (UPawnModule* Module = Pawn ? UPawnModule::GetPtr(Pawn->GetWorld()) : nullptr) Module->AddPawnToList(Pawn);
}

void UPawnModuleStatics::RemovePawnFromList(APawnBase* Pawn)
{
	if (UPawnModule* Module = Pawn ? UPawnModule::GetPtr(Pawn->GetWorld()) : nullptr) Module->RemovePawnFromList(Pawn);
}
