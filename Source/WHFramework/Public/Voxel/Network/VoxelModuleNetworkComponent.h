#pragma once

#include "CoreMinimal.h"
#include "Main/Base/ModuleNetworkComponentBase.h"
#include "Voxel/Network/VoxelNetworkTransfer.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "VoxelModuleNetworkComponent.generated.h"

class APlayerController;
class UVoxelModule;
struct FVoxelOverlayNetworkState;
struct FVoxelRepresentationSubscriptionState;
struct FVoxelOverlayRegionMessage;
struct FVoxelOverlayQueuedPacket;
struct FVoxelTaskResult;
class FVoxelNetworkEnvelopeDecode;
class FVoxelTerrainFrameContext;
struct FVoxelViewSettings;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoxelProtocolFailure, const FString&, Reason);
DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelIntentReplyReceived, const FVoxelEditReply&);
DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelRepresentationReplyReceived, const FVoxelRepresentationReply&);
DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelRepresentationInvalidated, const FVoxelRepresentationInvalidate&);

struct WHFRAMEWORK_API FVoxelTerrainNetworkStatus
{
	bool bActive = false;
	int32 Phase = 0;
	int32 Regions = 0;
	uint64 BuildSerial = 0;
	uint64 PacketBytes = 0;
	uint64 EnvelopeBytes = 0;
	uint64 InboundBytes = 0;
	bool bSubscriptionPending = false;
	bool bBuilding = false;
	bool bEncoding = false;
	bool bDecoding = false;
	bool bNeedsSnapshot = false;
	bool bSnapshotTerminalFailure = false;
	int32 SubscriptionAdmission = 0;
	int32 SnapshotAdmission = 0;
	int32 EncoderAdmission = 0;
	int32 OutboundAdmission = 0;
	uint64 AuthorityEpoch = 0;
	FGuid DirectoryEpoch;
	FGuid Session;
	FGuid Subscription;
	FGuid Ticket;
};

UCLASS()
class WHFRAMEWORK_API UVoxelModuleNetworkComponent : public UModuleNetworkComponentBase
{
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelNetworkWorldControlTest;
#endif
	GENERATED_BODY()

public:
	UVoxelModuleNetworkComponent();
	UVoxelModuleNetworkComponent(FVTableHelper& Helper);
	virtual ~UVoxelModuleNetworkComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type InReason) override;
	virtual void TickComponent(
		float InDeltaSeconds,
		ELevelTick InTickType,
		FActorComponentTickFunction* InFunction) override;

	uint64 AllocateRequestId();
	bool SubmitIntent(const FVoxelEditIntent& InIntent);
	bool RequestRepresentation(
		EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey,
		uint64 InKnownRevision);
	bool IsSessionReady() const;
	FVoxelTerrainNetworkStatus GetTerrainNetworkStatus() const;
	void AdvanceTerrainNetwork(FVoxelTerrainFrameContext& InFrame);
	bool AdvanceDeferredWorldControl();
	void OnTerrainWorldStopping();
	void SetAuthorizedObserver(AActor* InObserver, bool bInAllowBuild);
	AActor* GetAuthorizedObserver() const;

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

private:
	void BindModule();
	static bool IsRepresentationInRange(const FVector& InObserverCell, EVoxelRepresentationWireType InType,
		const FVoxelGenerationBounds& InBounds, const FVoxelViewSettings& InSettings);
	void ResetProtocol();
	void Fail(const FString& InReason);
	bool Send(EVoxelMessage InKind, TConstArrayView<uint8> InPayload, EVoxelTransferPriority InPriority,
		const FVoxelGenerationConsumerKey* InConsumer = nullptr, FString* OutError = nullptr);
	bool EnsureNetworkState(FString& OutError);
	bool EnsureTransferResources(FString& OutError);
	void ProcessBootstrapPacket(const FVoxelRPCPacket& InPacket, bool bInServer);
	bool HandleHelloPayload(TConstArrayView<uint8> InPayload);
	bool HandleWelcomePayload(TConstArrayView<uint8> InPayload, const FGuid& InSession);
	void HandleReadyPayload(TConstArrayView<uint8> InPayload);
	void DeferIngressResync(const FVoxelGenerationConsumerKey& InConsumer, uint64 InBeforeAdmission, bool bInServer, bool bInAwaitRelease = true);
	bool FinishPendingWelcome();
	void SendHello(double InNow);
	bool FinishPendingReady();
	void Receive(const FVoxelRPCPacket& InPacket, bool bInServer);
	bool ProcessPacket(const FVoxelOverlayQueuedPacket& InPacket);
	void AdvanceEnvelopeDecode(double InNow);
	bool StageDecodedEnvelope(const TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe>& InWork,
		FVoxelTaskResult&& InResult);
	bool ApplyDecodedEnvelope(bool bInWorldControl);
	bool AdvanceDecodedInvalidation();
	bool HandleWelcome(const FVoxelWireMessage& InMessage);
	void AdvancePendingRepresentations(double InDeadline);
	void AdvanceRepresentationSubscriptions(double InNow, double InDeadline);
	bool ReserveRepresentationTables();
	void ResetRepresentationRequests();
	bool TryApplyRepresentationReply(uint64 InSerial);
	void Handle(const FVoxelWireMessage& InMessage, bool bInServer,
		const FVoxelRepresentationReply* InPreparedReply = nullptr);
	void RefreshInterest(double InNow);
	void AdvanceOverlayProtocol(double InNow, double InDeadline);
	void RefreshOverlaySubscription(double InNow);
	void BeginOverlaySnapshot();
	void DecodeOverlayMessage(const FVoxelWireMessage& InMessage, bool bInServer);
	void HandleOverlayMessage(EVoxelMessage InKind, FVoxelOverlayRegionMessage&& InMessage, bool bInServer);
	void CaptureOverlayCommit(const FVoxelEditBatch& InBatch);
	bool SendOverlayControl(EVoxelMessage InKind, TConstArrayView<FIntVector> InRegions);
	void OnCommit(const FVoxelEditBatch& InBatch);
	APlayerController* Controller() const;

private:
	UPROPERTY(EditAnywhere, Category = "Voxel|Network")
	FVoxelNetworkSettings NetworkSettings;

	TWeakObjectPtr<UVoxelModule> Module;
	TWeakObjectPtr<AActor> AuthorizedObserver;
	FVoxelNetworkTransfer Transfer;
	TSharedPtr<FVoxelOverlayNetworkState, ESPMode::ThreadSafe> OverlayNetwork;
	TSet<FIntVector> GameplayInterest;
	TUniquePtr<FVoxelRepresentationSubscriptionState> RepresentationSubscriptions;
	struct FPendingRepresentation
	{
		FVoxelRepresentationRequest Request;
		FVoxelRepresentationReply Reply;
		FGuid RequestSession;
		uint64 WorldEpoch = 0;
		uint64 OwnerIncarnation = 0;
		uint64 CapacityVersion = 0;
		double LastSentSeconds = 0.0;
		bool bHasReply = false;
	};
	TMap<uint64, FPendingRepresentation> PendingRepresentationRequests;
	int32 PendingRepresentationCursor = 0;
	uint64 PendingRepresentationBytes = 0;
	int32 RepresentationTableCapacity = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RepresentationTableLease;
	FGuid Session;
	FGuid SourceId;
	FDelegateHandle CommitHandle;
	uint64 ClientNonce = 0;
	uint64 NextOutboundId = 1;
	FGuid TaskOwnerId = FGuid::NewGuid();
	uint64 TaskOwnerIncarnation = 1;
	uint64 NextTaskSerial = 0;
	FVoxelGenerationConsumerKey NetworkStateConsumer;
	FVoxelGenerationConsumerKey NetworkTransferConsumer;
	FVoxelGenerationConsumerKey NetworkStateCapacityConsumer;
	uint64 NetworkStateCapacityVersion = 0;
	bool bNetworkStateWaitingCapacity = false;
	bool bNetworkTransferBound = false;
	FVoxelGenerationConsumerKey PendingWelcomeConsumer;
	FGuid PendingWelcomeSession;
	uint64 PendingWelcomeNonce = 0;
	uint64 PendingWelcomeCapacityVersion = 0;
	uint64 HelloCapacityVersion = 0;
	bool bPendingWelcome = false;
	bool bPendingWelcomeWaitingCapacity = false;
	bool bHelloWaitingCapacity = false;
	FVoxelGenerationConsumerKey PendingReadyConsumer;
	FGuid PendingReadySession;
	uint64 PendingReadyCapacityVersion = 0;
	bool bPendingReady = false;
	bool bPendingReadyServer = false;
	bool bPendingReadyWaitingCapacity = false;
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

