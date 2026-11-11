#include "Main/WorldNetworkRecipients.h"

#include "Engine/ChildConnection.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

TArray<APlayerController*> WorldNetworkRecipients::Gather(UWorld* InWorld)
{
	TArray<APlayerController*> Result;
	if (!InWorld || InWorld->bIsTearingDown || InWorld->GetNetMode() == NM_Client) return Result;
	TSet<UNetConnection*> Seen;
	for (FConstPlayerControllerIterator It = InWorld->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		UNetConnection* Connection = Player && !Player->IsLocalController() ? Player->GetNetConnection() : nullptr;
		if (!Connection) continue;
		if (UChildConnection* Child = Connection->GetUChildConnection()) Connection = Child->GetParentConnection();
		if (!Connection || Seen.Contains(Connection)) continue;
		APlayerController* Parent = Connection->PlayerController;
		if (!IsValid(Parent) || Parent->GetWorld() != InWorld || Parent->IsLocalController()) continue;
		Seen.Add(Connection);
		Result.Add(Parent);
	}
	return Result;
}
