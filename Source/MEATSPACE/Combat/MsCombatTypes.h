#pragma once

#include "CoreMinimal.h"
#include "MsCombatTypes.generated.h"

/**
 * Which weapon the character currently has out.
 *
 * The design split: gun handles flying clankers, sword handles ground clankers. Switching
 * between them under pressure is meant to be part of the skill of MEATSPACE, so the swap
 * itself needs to feel instant.
 */
UENUM(BlueprintType)
enum class EMsWeaponSlot : uint8
{
	/** Anti-air. Hitscan, ranged. */
	Gun		UMETA(DisplayName = "Gun"),

	/** Anti-ground. Swept arc, close range, hits multiple targets. */
	Sword	UMETA(DisplayName = "Sword")
};
