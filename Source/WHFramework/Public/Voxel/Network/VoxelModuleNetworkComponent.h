#pragma once

#include "CoreMinimal.h"
#include "Main/Base/ModuleNetworkComponentBase.h"
#include "Voxel/Network/VoxelNetworkTransfer.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "VoxelModuleNetworkComponent.generated.h"

class APawn;
class APlayerController;
class UVoxelModule;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoxelProtocolFailure, const FString&, Reason);
DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelIntentReplyReceived, const FVoxelEditReply&);
DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelRepresentationReplyReceived, const FVoxelRepresentationReply&);
DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelRepresentationInvalidated, const FVoxelRepresentationInvalidate&);

UCLASS()
class WHFRAMEWORK_API UVoxelModuleNetworkComponent : public UModuleNetworkComponentBase
{
	GENERATED_BODY()

public:
	UVoxelModuleNetworkComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type InReason) override;
	virtual void TickComponent(
		float InDeltaSeconds,
		ELevelTick InTickType,
		FActorComponentTickFunction* InFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	uint64 AllocateRequestId();
	bool SubmitIntent(const FVoxelEditIntent& InIntent);
	bool RequestRepresentation(
		EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey,
		uint64 InKnownRevision);
	bool IsSessionReady() const;
	void SetAuthorizedObserver(AActor* InObserver, bool bInAllowBuild);
	AActor* GetAuthorizedObserver() const;
	static void PushInventoryFor(AActor* InOwner);

	UPROPERTY(BlueprintAssignable)
	FVoxelProtocolFailure OnProtocolFailure;

	FVoxelIntentReplyReceived OnIntentReply;
	FVoxelRepresentationReplyReceived OnRepresentationReply;
	FVoxelRepresentationInvalidated OnRepresentationInvalidated;

protected:
	UFUNCTION(Server, Reliable)
	void ServerReceive(const FVoxelRPCPacket& InPacket);

	UFUNCTION(Client, Reliable)
	void ClientReceive(const FVoxelRPCPacket& InPacket);

	UPROPERTY(Replicated)
	TObjectPtr<APawn> InventoryPawn;

	UPROPERTY(Replicated)
	uint64 InventoryEpoch = 0;

private:
	void BindModule();
	void ResetProtocol();
	void Fail(const FString& InReason);
	bool Send(EVoxelMessage InKind, const TArray<uint8>& InPayload, EVoxelTransferPriority InPriority);
	void Receive(const FVoxelRPCPacket& InPacket, bool bInServer);
	void Handle(const FVoxelWireMessage& InMessage, bool bInServer);
	void RefreshInterest(double InNow);
	void SendSectionState(const FIntVector& InSection);
	void SendSectionSnapshot(const FIntVector& InSection);
	void ApplyPendingSnapshots();
	void OnCommit(const FVoxelEditBatch& InBatch);
	APlayerController* Controller() const;

private:
	UPROPERTY(EditAnywhere, Category = "Voxel|Network")
	FVoxelNetworkSettings NetworkSettings;

	TWeakObjectPtr<UVoxelModule> Module;
	TWeakObjectPtr<AActor> AuthorizedObserver;
	FVoxelNetworkTransfer Transfer;
	TMap<FIntVector, FVoxelNetworkSectionSnapshot> PendingSnapshots;
	TSet<FIntVector> GameplayInterest;
	TMap<FIntVector, uint64> SentChangeSummaryRevisions;
	TMap<FVoxelRepresentationWireKey, uint8> RepresentationSubscriptions;
	TMap<uint64, FVoxelRepresentationRequest> PendingRepresentationRequests;
	FGuid Session;
	FGuid SourceId;
	FDelegateHandle CommitHandle;
	uint64 ClientNonce = 0;
	uint64 NextOutboundId = 1;
	uint64 LastInboundEditId = 0;
	double LastHello = -10.0;
	double LastInterest = -10.0;
	double PacketWindow = 0.0;
	int32 PacketsInWindow = 0;
	bool bObserverCanBuild = false;
	bool bReady = false;
	bool bWelcomed = false;
	bool bRejected = false;
};
