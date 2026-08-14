#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MsGrenadeComponent.generated.h"

class AMsGrenade;

/**
 * Throws grenades. A component rather than character code, because the class system will
 * eventually want abilities that slot in and out per class.
 *
 * Server-authoritative: the client asks, the server spawns, and the grenade replicates back.
 * No local prediction - a thrown object arriving a few frames late is invisible, unlike a
 * hitscan shot or a melee swing where the delay is felt immediately.
 */
UCLASS(ClassGroup = (Meatspace), meta = (BlueprintSpawnableComponent))
class MEATSPACE_API UMsGrenadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMsGrenadeComponent();

	UFUNCTION(BlueprintCallable, Category = "Meatspace|Grenade")
	void ThrowGrenade();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Grenade")
	bool IsReady() const;

	/** 0 = ready, 1 = just thrown. Drives the HUD readiness pip. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Grenade")
	float GetCooldownFraction() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade")
	TSubclassOf<AMsGrenade> GrenadeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "0.0"))
	float Cooldown = 6.0f;

	/** Throw speed in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "1.0"))
	float ThrowSpeed = 1700.0f;

	/** How much upward arc to add. 0 throws flat, higher lobs it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ThrowArc = 0.4f;

	/** Spawn offset from the thrower's origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade")
	FVector ThrowOffset = FVector(60.0f, 0.0f, 60.0f);

protected:
	UFUNCTION(Server, Reliable)
	void ServerThrow(const FVector_NetQuantize& Start, const FVector_NetQuantizeNormal& Direction);

private:
	/** Tracked separately per side so a client cannot spam faster than the cooldown. */
	float LastThrowTime = -1000.0f;
	float ServerLastThrowTime = -1000.0f;
};
