// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeavyHanded : ModuleRules
{
	public HeavyHanded(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",

			// GAS — 역할별 스킬 4슬롯, 쿨다운, 상태(과적/다운/변장), GameplayCue 연출
			"GameplayTags", "GameplayTasks", "GameplayAbilities",

			// 물리 운반 + 노획물 리플리케이션
			"PhysicsCore", "NetCore",

			// 경비 순찰 · 조사 이동 · 시야 판정
			"AIModule", "NavigationSystem",

			// HUD, 경계도 게이지, 경비 인지 게이지, 은신처 상점
			"UMG",

			// 스킬 시각 효과
			"Niagara",

			// 소음 파라미터 프로젝트 세팅
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate", "SlateCore",

			// 로비 / 세션 (현재 OnlineSubsystemNull — LAN + 직접 IP)
			"OnlineSubsystem", "OnlineSubsystemUtils",

			// 소음 SFX는 폴리싱이 아니라 기능. 초반부터 배치한다
			"AudioMixer"
		});
	}
}
