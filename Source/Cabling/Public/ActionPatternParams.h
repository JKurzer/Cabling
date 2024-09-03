#include "FActionBitMask.h"
#include "SkeletonTypes.h"

struct FActionPatternParams
{
public:
	//this specifies a parametric bind's "preference" which will need to become an int for priority.
	//and if the binding consumes the input or passes it through.
	// an example is that we WILL want to say that holding down the trigger should be fired before
	// single press. actually, we might do pattern-priority rather than anything else.
	// hard to say. there is a trick here that could let us handle firing a diff ability if the ability is on cool down but I'm not borrowing trouble.
	bool preferToMatch = false;
	bool consumeInput = true;
	bool defaultBehavior = false;
	bool FiresCosmetics = false;

	FGunKey ToFire; //IT WAS A MISTAKE. OH NO.
	FActionBitMask ToSeek;
	InputStreamKey MyInputStream;
	FireControlKey MyOrigin;
	FActionPatternParams(const FActionBitMask ToSeek_In, FireControlKey MyOrigin_In, InputStreamKey MyInputStream_In, FGunKey Fireable) :
		ToSeek(ToSeek_In), MyInputStream(MyInputStream_In), MyOrigin(MyOrigin_In)
	{
		ToFire = Fireable;
	};

	friend uint32 GetTypeHash(const FActionPatternParams& Other)
	{
		// it's probably fine!
		return GetTypeHash(Other.ToFire) + GetTypeHash(Other.ToSeek);
	}
	
};

static bool operator==(FActionPatternParams const& lhs, FActionPatternParams const& rhs) {
	return lhs.ToFire == rhs.ToFire;
}