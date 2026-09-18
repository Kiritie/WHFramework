#pragma once
#include "CoreMinimal.h"
#include "Main/Base/ModuleNetworkComponentBase.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Network/VoxelNetworkTransfer.h"
#include "Voxel/Network/VoxelPhase2Network.h"
#include "VoxelModuleNetworkComponent.generated.h"
class UVoxelModule;class APawn;class APlayerController;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVoxelProtocolFailure,const FString&,Reason);
DECLARE_MULTICAST_DELEGATE_OneParam(FVoxelIntentReplyReceived,const FVoxelEditReply&);
UCLASS()
class WHFRAMEWORK_API UVoxelModuleNetworkComponent:public UModuleNetworkComponentBase
{
    GENERATED_BODY()
public:
    UVoxelModuleNetworkComponent();
    virtual void BeginPlay()override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const override;
    uint64 AllocateRequestId(){return NextOutboundId==MAX_uint64?0:NextOutboundId++;}
    bool SubmitIntent(const FVoxelEditIntent& Intent);
    bool RequestProxy(const FVoxelProxyRequest& Request);
    bool DeliverProxy(const FVoxelProxyReply& Reply);
    bool IsSessionReady()const{return bReady&&!bRejected;}
    void SetAuthorizedObserver(AActor* Observer,bool bAllowBuild);
    AActor* GetAuthorizedObserver()const;
    void CompleteSnapshotSend(FGuid BatchId,TArray<uint8>&& Encoded,bool bSuccess);
    static void PushInventoryFor(AActor* Owner);
    UPROPERTY(BlueprintAssignable) FVoxelProtocolFailure OnProtocolFailure;
    FVoxelIntentReplyReceived OnIntentReply;
protected:
    UFUNCTION(Server,Reliable) void ServerReceive(const FVoxelRPCPacket& Packet);
    UFUNCTION(Client,Reliable) void ClientReceive(const FVoxelRPCPacket& Packet);
    UPROPERTY(Replicated) TObjectPtr<APawn> InventoryPawn;
    UPROPERTY(Replicated) uint64 InventoryEpoch=0;
private:
    void BindModule();
    void ResetProtocol();
    void Fail(const FString& Reason);
    bool Send(EVoxelMessage Kind,const TArray<uint8>& Payload);
    void Receive(const FVoxelRPCPacket& Packet,bool bServer);
    void Handle(const FVoxelWireMessage& Message,bool bServer);
    void RefreshInterest();
    bool QueueSnapshots(const TArray<FVoxelSectionKey>& Keys,bool bAtomic=true);
    void OnCommit(const FVoxelEditBatch& Batch);
    void OnRemoteCompleted(const FVoxelSnapshotBatch& Batch,bool bSuccess);
    void SendInventory();
    bool ApplyInventory(const TArray<uint8>& Payload);
    void ResetPhase2();
    void PumpPhase2();
    void SendFineInterest();
    bool HandlePhase2(const FVoxelWireMessage& Message,bool bServer);
    void NotifyProxyEdit(const FVoxelEditBatch& Batch);
    APlayerController* Controller()const;
    TWeakObjectPtr<UVoxelModule> Module;
    TWeakObjectPtr<AActor> AuthorizedObserver;
    bool bObserverCanBuild=false,bReady=false,bWelcomed=false,bRejected=false;
    FGuid Session,Nonce,SourceId;
    FDelegateHandle CommitHandle,RemoteHandle;
    FVoxelNetworkTransfer Transfer;
    FVoxelPhase2NetworkState Phase2;
    TSet<FVoxelSectionKey> Interest;
    TMap<FVoxelSectionKey,uint64> Acknowledged;
    TMap<FVoxelSectionKey,double> LastSent;
    struct FPendingSnapshot
    {
        TMap<FVoxelSectionKey,uint64> Revisions;
        double CreatedAt=0,SentAt=0;
        bool bSent=false,bAtomic=false;
    };
    TMap<FGuid,FPendingSnapshot> PendingBatches;
    TArray<TArray<FVoxelSectionKey>> DirtyTransactions;
    TArray<uint8> PendingInventory;
    uint64 PendingInventoryEpoch=0,LastRequestId=0,NextOutboundId=1;
    double LastHello=-10,LastInterest=-10,LastInventory=-10,LastReceiveTick=0,EditTokens=40;
    double PacketWindow=0;int32 PacketsInWindow=0;
};
