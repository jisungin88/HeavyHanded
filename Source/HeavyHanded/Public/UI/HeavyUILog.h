#pragma once

#include "CoreMinimal.h"

/**
 * UI · HUD · 위젯 전용 로그 카테고리 (문서 07 테스트 5장).
 *
 * [왜 헤더에 EXTERN 인가] 기본은 .cpp 안의 DEFINE_LOG_CATEGORY_STATIC 이다.
 *   헤더에 노출하면 아무나 이 카테고리로 찍어서 필터의 의미가 사라지기 때문이다.
 *   다만 UI 는 위젯 클래스가 계속 늘어나고(경계도 · 인지 게이지 · 타이머 · 결과 화면)
 *   전부 같은 종류의 사건을 낸다 — "무엇에 붙지 못했는가", "무엇이 비어 있는가".
 *   카테고리가 갈리면 "HUD 에 무슨 일이 있었나" 를 한 필터로 따라갈 수 없다.
 *   노획물이 LogLoot 를, AI 가 BT 노드 7개 때문에 LogGuardAI 를 EXTERN 으로 둔 것과 같다.
 *
 * [STATIC 을 쓰면 안 되는 이유] DEFINE_LOG_CATEGORY_STATIC 은 카테고리 이름으로
 *   struct 타입을 만든다. UBT 가 여러 .cpp 를 하나로 합쳐 빌드하므로(unity build)
 *   같은 이름을 두 파일에서 STATIC 으로 정의하면 그 둘이 같은 번역 단위에 들어가
 *   "struct 재정의" 로 빌드가 깨진다. 위젯이 둘 이상 되는 순간 반드시 터진다.
 *
 * [무엇을 찍나] UI 는 실패해도 예외가 나지 않는다. 구독 대상에 못 붙거나
 *   BindWidget 대상이 비어 있으면 "게이지가 안 움직인다" 로만 드러난다.
 *   조용히 return 하는 지점마다 이유를 남긴다.
 *
 * 실제 정의(DEFINE_LOG_CATEGORY)는 PerceptionMeterWidget.cpp 에 한 번만 있다.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogHeavyUI, Log, All);
