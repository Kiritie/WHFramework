#pragma once
#include "CoreMinimal.h"
#include "Zip/zip.h"

class WHFRAMEWORKCORE_API FZipManager
{
    enum EStatus : uint8
    {
        Doing = 0,
        Succeeded,
        Failed
    };
    static bool IsUTF8(const char* InChars);
    //抄自 https://www.codeleading.com/article/70314710113/#google_vignette
    static FString ANSIToUTF8ToFString(const char* InChars);
    static bool Pack(zip* InArchive, const FString& InDirectory, const FString& InRelativePath, TArray<TSharedPtr<EStatus>>& Async);
    static bool Unpack(const FString& InZipFile, const FString& InDestDirectory, int InIndex, int InStep);
public:
    FZipManager() = default;
    UFUNCTION()
    static bool IsZip(const FString& InZipFile);
    /**
     * @brief 将目录打包为指定Zip文件
     * 注意 如果ToPath已经存在，则打包失败。
     * @param InZipFile zip包的文件路径
     * @param InOriginDirectory 需要打包目录
     * @param InIgnoreFile 打包配置文件
     * @return 打包是否成功
     */
    UFUNCTION()
    static bool Pack(const FString& InZipFile, const FString& InOriginDirectory, const FString& InIgnoreFile = FString());
    /**
	 * @brief 将文件们基于传入的根路径打包为指定Zip文件
	 * 注意: InFileRootDirectory 为空时,所有文件都将打入根目录
	 * @param InZipFile zip包的文件路径
	 * @param InOriginFiles 打包的文件们
	 * @param InRootDirectory 文件们的根路径
	 * @return 打包是否成功
	 */
    static bool Pack(const FString& InZipFile,const TArray<FString>& InOriginFiles, const FString& InRootDirectory = FString());
    /**
     * @brief 将指定文件解压到目标目录
     * @param InZipFile 
     * @param InDestDirectory 
     * @return 
     */
    UFUNCTION()
    static bool Unpack(const FString& InZipFile, const FString& InDestDirectory);
};
