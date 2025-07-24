#pragma once
#include "WHFrameworkCoreTypes.h"
#include "Templates/MaxSizeof.h"

//////////////////////////////////////////////////////////////////////////
// GUID
namespace AssetPrivate
{
	struct WHFRAMEWORKCORE_API FGUIDHelper
	{
		static FGuid EmptyGUID;
		static TMap<FName, FGuid> GUIDMap;

		static FGuid NewGUID(const FName& Name);
	};
}

#define EMPGUID AssetPrivate::FGUIDHelper::EmptyGUID

#define NEWGUID(Name) AssetPrivate::FGUIDHelper::NewGUID(#Name)

/*
 * FVigmaAssetID
 */
struct WHFRAMEWORKCORE_API FUniqueAssetID
{
public:
	using FUniqueID = uint32;

	enum class EType : uint8 { Object, UniqueID, Guid, Name, Null };

	/** Default constructed null item ID */
	FUniqueAssetID() : Type(EType::Null), CachedHash(0) {}

	/** ID representing a UObject */
	FUniqueAssetID(const UObject* InObject) : Type(EType::Object)
	{
		check(InObject);
		new (Data) FObjectKey(InObject);
		CachedHash = CalculateTypeHash();
	}
	FUniqueAssetID(const FObjectKey& InKey) : Type(EType::Object)
	{
		new (Data) FObjectKey(InKey);
		CachedHash = CalculateTypeHash();
	}

	/** ID representing a generic tree item */
	FUniqueAssetID(const FUniqueID& CustomID) : Type(EType::UniqueID)
	{
		new (Data) FUniqueID(CustomID);
		CachedHash = CalculateTypeHash();
	}

	FUniqueAssetID(const FGuid& InGuid) : Type(EType::Guid)
	{
		new (Data) FGuid(InGuid);
		CachedHash = CalculateTypeHash();
	}

	FUniqueAssetID(const TCHAR* InName) : Type(EType::Name)
	{
		new (Data) FName(InName);
		CachedHash = CalculateTypeHash();
	}

	FUniqueAssetID(const FName& InName) : Type(EType::Name)
	{
		new (Data) FName(InName);
		CachedHash = CalculateTypeHash();
	}

	/** Copy construction / assignment */
	FUniqueAssetID(const FUniqueAssetID& Other)
	{
		*this = Other;
	}
	FUniqueAssetID& operator=(const FUniqueAssetID& Other)
	{
		Type = Other.Type;
		switch(Type)
		{
			case EType::Object:			new (Data) FObjectKey(Other.GetAsObjectKey());		break;
			case EType::Guid:			new (Data) FGuid(Other.GetAsGuid());				break;
			case EType::Name:			new (Data) FName(Other.GetAsName());				break;
			case EType::UniqueID:		new (Data) FUniqueID(Other.GetAsHash());			break;
			default:																		break;
		}

		CachedHash = CalculateTypeHash();
		return *this;
	}

	/** Move construction / assignment */
	FUniqueAssetID(FUniqueAssetID&& Other)
	{
		*this = MoveTemp(Other);
	}
	FUniqueAssetID& operator=(FUniqueAssetID&& Other)
	{
		Swap(*this, Other);
		return *this;
	}

	~FUniqueAssetID()
	{
		switch(Type)
		{
			case EType::Object:			GetAsObjectKey().~FObjectKey();							break;
			case EType::Guid:			GetAsGuid().~FGuid();									break;
			case EType::Name:			GetAsName().~FName();									break;
			case EType::UniqueID:		/* NOP */												break;
			default:																			break;
		}
	}

	bool operator==(const FUniqueAssetID& Other) const
	{
		return Type == Other.Type && CachedHash == Other.CachedHash && Compare(Other);
	}
	bool operator!=(const FUniqueAssetID& Other) const
	{
		return Type != Other.Type || CachedHash != Other.CachedHash || !Compare(Other);
	}

	bool IsValid() const
	{
		switch(Type)
		{
			case EType::Object:			return GetAsObjectKey().ResolveObjectPtr() != nullptr;
			case EType::Guid:			return GetAsGuid().IsValid();
			case EType::Name:			return GetAsName().IsValid();
			case EType::UniqueID:		return GetAsHash() > 0;
			default:					break;
		}
		return false;
	}

	FString ToString() const
	{
		switch(Type)
		{
			case EType::Object:			return FString::Printf(TEXT("ObjectKey_%d"), GetTypeHash(GetAsObjectKey()));
			case EType::Guid:			return FString::Printf(TEXT("GUID_%s"), *GetAsGuid().ToString());
			case EType::Name:			return FString::Printf(TEXT("Name_%s"), *GetAsName().ToString());
			case EType::UniqueID:		return FString::Printf(TEXT("UniqueID_%d"), GetAsHash());
			default:					break;
		}
		return TEXT("");
	}

	uint32 CalculateTypeHash() const
	{
		uint32 Hash = 0;
		switch(Type)
		{
			case EType::Object:			Hash = GetTypeHash(GetAsObjectKey());				break;
			case EType::Guid:			Hash = GetTypeHash(GetAsGuid());					break;
			case EType::Name:			Hash = GetTypeHash(GetAsName());					break;
			case EType::UniqueID:		Hash = GetTypeHash(GetAsHash());					break;
			default:																		break;
		}

		return HashCombine((uint8)Type, Hash);
	}

	friend uint32 GetTypeHash(const FUniqueAssetID& ItemID)
	{
		return ItemID.CachedHash;
	}

public:
	static FUniqueAssetID EmptyObjectKey;
	static FUniqueAssetID EmptyGuid;
	static FUniqueAssetID EmptyName;
	static FUniqueAssetID EmptyUniqueID;

public:
	FObjectKey& 	GetAsObjectKey() const 		{ return *reinterpret_cast<FObjectKey*>(Data); }
	FGuid&			GetAsGuid() const			{ return *reinterpret_cast<FGuid*>(Data); }
	FName&		GetAsName() const			{ return *reinterpret_cast<FName*>(Data); }
	FUniqueID&		GetAsHash() const			{ return *reinterpret_cast<FUniqueID*>(Data); }

private:
	/** Compares the specified ID with this one - assumes matching types */
	bool Compare(const FUniqueAssetID& Other) const
	{
		switch(Type)
		{
			case EType::Object:			return GetAsObjectKey() == Other.GetAsObjectKey();
			case EType::Guid:			return GetAsGuid() == Other.GetAsGuid();
			case EType::Name:			return GetAsName() == Other.GetAsName();
			case EType::UniqueID:		return GetAsHash() == Other.GetAsHash();
			case EType::Null:			return true;
			default: check(false);		return false;
		}
	}

	EType Type;

	uint32 CachedHash;
	static const uint32 MaxSize = TMaxSizeof<FObjectKey, FGuid, FName, FUniqueID>::Value;
	mutable uint8 Data[MaxSize];
};

/*
 * FUniqueAssetData
 */
class WHFRAMEWORKCORE_API FUniqueAssetData : public FUniqueClass
{
public:
	FUniqueAssetData(FUniqueType InType)
		: FUniqueClass(InType)
	{
	}

	virtual ~FUniqueAssetData() override {}

	static const FUniqueType Type;

public:
	virtual bool IsValid() const { return false; }

	virtual void ResetRuntimeData() { }

public:
	virtual FUniqueAssetID GetID() const { return FUniqueAssetID(); }

	virtual FString GetDisplayString() const { return TEXT(""); }

	virtual const FSlateBrush* GetDisplayIcon() const { return nullptr; }

	virtual TSharedPtr<FJsonObject> ToJsonObject() const { return nullptr; }
};

/*
 * FUniqueAssetItem
 */
class WHFRAMEWORKCORE_API FUniqueAssetItem : public FUniqueClass, public TSharedFromThis<FUniqueAssetItem>
{
protected:
	FUniqueAssetItem(FUniqueType InType, const FUniqueAssetID& InAssetID)
		: FUniqueClass(InType),
		  AssetID(InAssetID)
	{}
	virtual ~FUniqueAssetItem() override {}

	static const FUniqueType Type;

	FUniqueAssetID AssetID;

public:
	template <typename AssetItemType, typename AssetItemData>
	static TSharedPtr<AssetItemType> CreateItemFor(const AssetItemData& Data)
	{
		TSharedPtr<AssetItemType> Item = MakeShareable(new AssetItemType(Data));
		Item->Initialize();
		return Item;
	}

public:
	virtual void Initialize() { }

	virtual bool IsValid() const { return AssetID.IsValid(); }

public:
	virtual FUniqueAssetID GetAssetID() const { return AssetID; }

	virtual FUniqueAssetData* GetAssetData() const;

	virtual FString GetDisplayString() const { return GetAssetData() ? GetAssetData()->GetDisplayString() : TEXT(""); }

	virtual FString GetToolTipString() const { return GetDisplayString(); }

	virtual const FSlateBrush* GetDisplayIcon() const { return GetAssetData() ? GetAssetData()->GetDisplayIcon() : nullptr; }
};

class FSimpleDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual bool Visit( const TCHAR* FilenameOrDirectory, bool bIsDirectory ) override
	{
		if (bIsDirectory)
		{
			Directories.Add(FilenameOrDirectory);
		}
		return true;
	}

public:
	TArray<FString> Directories;
};

class FSimpleFileVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual bool Visit( const TCHAR* FilenameOrDirectory, bool bIsDirectory ) override
	{
		if (!bIsDirectory)
		{
			Files.Add(FilenameOrDirectory);
		}
		return true;
	}

public:
	TArray<FString> Files;
};
