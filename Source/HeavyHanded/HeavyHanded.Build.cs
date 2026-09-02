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
			"DeveloperSettings",

			// FUniqueNetIdRepl — 레벨 이동을 건너 플레이어를 식별하는 유일한 키.
			// URunProgressSubsystem 이 공개 헤더에서 노출하므로 Public 이다
			"CoreOnline"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate", "SlateCore",

			// 로비 / 세션 (현재 OnlineSubsystemNull — LAN + 직접 IP)
			"OnlineSubsystem", "OnlineSubsystemUtils",

			// 소음 SFX는 폴리싱이 아니라 기능. 초반부터 배치한다
			"AudioMixer",

			// 레벨 이동 로딩 화면 (ULoadingScreenSubsystem).
			// 논심리스 트래블 중에는 게임 스레드가 LoadMap 안에서 멈춰 뷰포트 위젯이
			// 한 프레임도 그려지지 않는다. 별도 스레드로 그려 주는 창구가 이것뿐이다
			"MoviePlayer"
		});
	}
}
