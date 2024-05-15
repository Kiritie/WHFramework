#pragma once
#include "CoreMinimal.h"
#include "WHFrameworkCoreTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"
#include "Zip/zip.h"

class WHFRAMEWORKCORE_API FZipManager : public FManagerBase
{
	GENERATED_MANAGER(FZipManager)

public:
    enum EStatus : uint8
    {
        Doing = 0,
        Succeeded,
        Failed
    };
	
public:
	FZipManager();

	virtual ~FZipManager() override;
	
	static const FUniqueType Type;

public:
    bool Pack(const FString& InZipFile, const FString& InOriginDirectory, const FString& InIgnoreFile = TEXT(""));

    bool Unpack(const FString& InZipFile, const FString& InDestDirectory);

protected:
	bool Pack(const FString& InZipFile,const TArray<FString>& InOriginFiles, const FString& InRootDirectory = TEXT(""));

	bool Pack(zip* InArchive, const FString& InDirectory, const FString& InRelativePath, TArray<TSharedPtr<EStatus>>& Async);

	bool Unpack(const FString& InZipFile, const FString& InDestDirectory, int InIndex, int InStep);

public:
	bool IsZip(const FString& InZipFile);
	
	bool IsUTF8(const char* InChars);

	FString ANSIToUTF8ToFString(const char* InChars);
};
