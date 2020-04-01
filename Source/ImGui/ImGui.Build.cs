// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;
using System;
using System.Collections.Generic;

public class ImGui : ModuleRules
{
	public ImGui(ReadOnlyTargetRules Target) : base(Target)
	{

		bool bBuildEditor = Target.bBuildEditor;

		bool bEnableRuntimeLoader = true;

		PCHUsage = PCHUsageMode.UseSharedPCHs;

		PrivatePCHHeaderFile = "Private/ImGuiPrivatePCH.h";

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "../ThirdParty/ImGuiLibrary/Include")
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				"ImGui/Private",
				"ThirdParty/ImGuiLibrary/Private"
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"Core", "CoreUObject",
				"Engine",
				"InputCore",
				"OnlineSubsystem", "OnlineSubsystemUtils", "OnlineSubsystemNULL",
				"SignificanceManager",
				"PhysX", "APEX", "PhysXVehicles", "ApexDestruction",
				"AkAudio",
				"ReplicationGraph",
				"UMG",
				"AIModule",
				"NavigationSystem",
				"AssetRegistry",
				"GameplayTasks",
				"AnimGraphRuntime",
				"Slate", "SlateCore"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore"
			}
			);


		if (bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorStyle",
					"Settings",
					"UnrealEd",
				}
				);

			PublicDependencyModuleNames.AddRange(new string[] { "OnlineBlueprintSupport", "AnimGraph" });
		}

		PublicDependencyModuleNames.AddRange(new string[] { "FactoryGame", "SML" });

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);

		PrivateDefinitions.Add(string.Format("RUNTIME_LOADER_ENABLED={0}", bEnableRuntimeLoader ? 1 : 0));
	}
}
