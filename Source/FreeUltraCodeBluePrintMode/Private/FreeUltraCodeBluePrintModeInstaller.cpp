// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModeInstaller.h"
#include "FreeUltraCodeBluePrintModeLog.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

FString FFreeUltraCodeBluePrintModeInstaller::ResolveDefaultTargetDir(const FString& /*SourceDir*/)
{
	// 默认安装目录固定为命名空间化插件名，避免用源目录名造成冲突。
	const FString LeafName = TEXT("FreeUltraCodeBluePrintMode");
	const FString ProjectPluginsDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins"));
	return FPaths::Combine(ProjectPluginsDir, LeafName);
}

int32 FFreeUltraCodeBluePrintModeInstaller::CopyDirectoryRecursive(const FString& From, const FString& To, bool bOverwrite, FString& OutError)
{
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

	if (!PF.DirectoryExists(*From))
	{
		OutError = FString::Printf(TEXT("源目录不存在：%s"), *From);
		return 0;
	}

	if (!PF.CreateDirectoryTree(*To))
	{
		OutError = FString::Printf(TEXT("无法创建目标目录：%s"), *To);
		return 0;
	}

	// 收集源目录下所有文件（相对路径），逐个拷贝。
	struct FCopyVisitor : public IPlatformFile::FDirectoryVisitor
	{
		IPlatformFile& PF;
		FString FromRoot;
		FString ToRoot;
		bool bOverwrite;
		int32 Count = 0;
		FString Error;

		FCopyVisitor(IPlatformFile& InPF, const FString& InFrom, const FString& InTo, bool InOverwrite)
			: PF(InPF), FromRoot(InFrom), ToRoot(InTo), bOverwrite(InOverwrite) {}

		virtual bool Visit(const TCHAR* FilenameOrDir, bool bIsDirectory) override
		{
			FString Rel = FilenameOrDir;
			FPaths::MakePathRelativeTo(Rel, *(FromRoot / TEXT("")));
			const FString Dest = FPaths::Combine(ToRoot, Rel);

			if (bIsDirectory)
			{
				if (!PF.CreateDirectoryTree(*Dest))
				{
					Error = FString::Printf(TEXT("创建子目录失败：%s"), *Dest);
					return false;
				}
			}
			else
			{
				if (PF.FileExists(*Dest))
				{
					if (!bOverwrite)
					{
						return true; // 跳过已存在
					}
					PF.DeleteFile(*Dest);
				}
				// 确保父目录存在
				PF.CreateDirectoryTree(*FPaths::GetPath(Dest));
				if (!PF.CopyFile(*Dest, FilenameOrDir))
				{
					Error = FString::Printf(TEXT("拷贝文件失败：%s -> %s"), FilenameOrDir, *Dest);
					return false;
				}
				++Count;
			}
			return true;
		}
	};

	FCopyVisitor Visitor(PF, FPaths::ConvertRelativePathToFull(From), FPaths::ConvertRelativePathToFull(To), bOverwrite);
	PF.IterateDirectoryRecursively(*FPaths::ConvertRelativePathToFull(From), Visitor);

	if (!Visitor.Error.IsEmpty())
	{
		OutError = Visitor.Error;
	}
	return Visitor.Count;
}

FFreeUltraCodeBluePrintModeInstallResult FFreeUltraCodeBluePrintModeInstaller::InstallFromLocal(const FString& SourceDir, const FString& TargetDir, bool bOverwrite)
{
	FFreeUltraCodeBluePrintModeInstallResult Result;

	const FString Source = FPaths::ConvertRelativePathToFull(SourceDir);
	const FString Target = TargetDir.IsEmpty()
		? ResolveDefaultTargetDir(SourceDir)
		: FPaths::ConvertRelativePathToFull(TargetDir);

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

	// 校验源目录是一个合法插件根（含 .uplugin）。
	bool bHasUplugin = false;
	PF.IterateDirectory(*Source, [&bHasUplugin](const TCHAR* Path, bool bDir)
	{
		if (!bDir && FPaths::GetExtension(Path).Equals(TEXT("uplugin"), ESearchCase::IgnoreCase))
		{
			bHasUplugin = true;
		}
		return true;
	});

	if (!bHasUplugin)
	{
		Result.bSuccess = false;
		Result.Message = FString::Printf(TEXT("源目录不是合法插件（未找到 .uplugin）：%s"), *Source);
		return Result;
	}

	// 防呆：源与目标相同则无需拷贝。
	if (FPaths::IsSamePath(Source, Target))
	{
		Result.bSuccess = true;
		Result.Message = FString::Printf(TEXT("源与目标相同，无需安装：%s"), *Target);
		return Result;
	}

	if (bOverwrite)
	{
		const FString LegacyDescriptor = FPaths::Combine(Target, TEXT("BlueprintMode.uplugin"));
		if (PF.FileExists(*LegacyDescriptor))
		{
			PF.DeleteFile(*LegacyDescriptor);
		}
	}

	FString Error;
	const int32 Copied = CopyDirectoryRecursive(Source, Target, bOverwrite, Error);

	if (!Error.IsEmpty())
	{
		Result.bSuccess = false;
		Result.FilesCopied = Copied;
		Result.Message = FString::Printf(TEXT("安装失败（已拷贝 %d 个文件）：%s"), Copied, *Error);
		return Result;
	}

	Result.bSuccess = true;
	Result.FilesCopied = Copied;
	Result.Message = FString::Printf(TEXT("安装成功：拷贝 %d 个文件到 %s。请重启编辑器以加载插件。"), Copied, *Target);
	UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("%s"), *Result.Message);
	return Result;
}

FFreeUltraCodeBluePrintModeInstallResult FFreeUltraCodeBluePrintModeInstaller::InstallFromGithub(const FString& RepoUrl, const FString& TargetDir, bool bOverwrite)
{
	FFreeUltraCodeBluePrintModeInstallResult Result;

	const FString Repo = RepoUrl.IsEmpty()
		? FString(TEXT("https://github.com/wellingfeng/ue-blueprint-mode.git"))
		: RepoUrl;

	const FString Target = TargetDir.IsEmpty()
		? ResolveDefaultTargetDir(TEXT(""))
		: FPaths::ConvertRelativePathToFull(TargetDir);

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

	if (PF.DirectoryExists(*Target))
	{
		if (!bOverwrite)
		{
			Result.bSuccess = false;
			Result.Message = FString::Printf(TEXT("目标目录已存在且未允许覆盖：%s"), *Target);
			return Result;
		}

		if (!PF.DeleteDirectoryRecursively(*Target))
		{
			Result.bSuccess = false;
			Result.Message = FString::Printf(TEXT("无法清空目标目录：%s"), *Target);
			return Result;
		}
	}

	if (!PF.CreateDirectoryTree(*FPaths::GetPath(Target)))
	{
		Result.bSuccess = false;
		Result.Message = FString::Printf(TEXT("无法创建父目录：%s"), *FPaths::GetPath(Target));
		return Result;
	}

	const FString GitExe = TEXT("git");
	const FString Params = FString::Printf(TEXT("clone --depth 1 \"%s\" \"%s\""), *Repo, *Target);
	int32 ReturnCode = 0;
	FString StdOut;
	FString StdErr;
	FPlatformProcess::ExecProcess(*GitExe, *Params, &ReturnCode, &StdOut, &StdErr);

	if (ReturnCode != 0)
	{
		Result.bSuccess = false;
		Result.Message = FString::Printf(TEXT("从 GitHub 下载失败（git clone 返回 %d）：%s"), ReturnCode, *StdErr);
		return Result;
	}

	const FString Descriptor = FPaths::Combine(Target, TEXT("FreeUltraCodeBluePrintMode.uplugin"));
	if (!PF.FileExists(*Descriptor))
	{
		Result.bSuccess = false;
		Result.Message = FString::Printf(TEXT("GitHub 仓库下载完成，但未找到插件描述文件：%s"), *Descriptor);
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("安装成功：已从 GitHub 下载到 %s。请重启编辑器以加载插件。"), *Target);
	UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("%s"), *Result.Message);
	return Result;
}
