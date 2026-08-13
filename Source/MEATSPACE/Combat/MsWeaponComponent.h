#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MsWeaponComponent.generated.h"

/**
 * Hitscan gun, server-authoritative.
 *
 * Netcode shape - this is the pattern every MEATSPACE weapon follows:
 *   1. The firing client plays its own muzzle/tracer FX immediately, so shooting feels
 *      instant regardless of ping. That is cosmetic only.
 *   2. The client tells the server where it was aiming (ServerFire).
 *   3. The SERVER does the trace and applies damage. It is the only authority on what died.
 *   4. The server multicasts the FX so every other player sees the shot too.
 *
 * A client can lie about its aim direction, so the server rate-limits independently. It never
 * trusts the client for anything except "I aimed roughly there".
 */
UCLASS(ClassGroup = (Meatspace), meta = (BlueprintSpawnableComponent))
class MEATSPACE_API UMsWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMsWeaponComponent();

	/** Begin firing. Holds down for automatic weapons. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Weapon")
	void StartFire();

	/** Stop firing. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Weapon")
	void StopFire();

	// --- Tunables. Daniel can change all of these in the Blueprint without a recompile. ---

	/** Damage per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Weapon")
	float Damage = 12.0f;

	/** Shots per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Weapon", meta = (ClampMin = "0.01"))
	float RoundsPerSecond = 8.0f;

	/** How far the shot reaches, in cm. 15000 = 150 m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Weapon", meta = (ClampMin = "1.0"))
	float Range = 15000.0f;

	/** Hold to keep firing, or one shot per click. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Weapon")
	bool bAutomatic = true;

	/** Cone of inaccuracy in degrees. 0 = perfectly accurate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Weapon", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float SpreadDegrees = 0.5f;

	/** Draw debug tracers. Turn off once we have real muzzle flash and tracer VFX. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Weapon|Debug")
	bool bDrawDebugShots = true;

protected:
	/** Fires a single round. Called directly, or on a timer while automatic. */
	void FireOnce();

	/** Client -> server: "I fired, aiming from here in this direction." */
	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize& TraceStart, const FVector_NetQuantizeNormal& AimDir);

	/** Server -> everyone: play the shot FX. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireFX(const FVector_NetQuantize& ImpactPoint, bool bHit);

	/** Shared trace used by both the local prediction and the authoritative server shot. */
	bool TraceShot(const FVector& Start, const FVector& Dir, FHitResult& OutHit) const;

	/** Cosmetic only. Never applies damage. */
	void PlayFireFX(const FVector& ImpactPoint, bool bHit);

	/** Where shots visually originate. Muzzle socket later; for now, roughly the camera. */
	FVector GetMuzzleLocation() const;

private:
	/** Local (predicted) fire timing. */
	float LastFireTime = -1000.0f;

	/** Server-side fire timing, tracked separately so a client cannot fire faster by lying. */
	float ServerLastFireTime = -1000.0f;

	FTimerHandle FireTimer;
};
