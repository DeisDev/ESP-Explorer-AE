#pragma once

#include "RE/H/hkHandle.h"

namespace RE
{
	class hknpMotionPropertiesIdBaseDiscriminant
	{
	public:
	};
	static_assert(std::is_empty_v<hknpMotionPropertiesIdBaseDiscriminant>);

	class hknpMotionPropertiesId :
		public hkHandle<std::uint16_t, 65535, hknpMotionPropertiesIdBaseDiscriminant>
	{
	public:
		enum class Preset : std::uint32_t
		{
			kStatic = 0x0,
			kDynamic = 0x1,
			kKeyframed = 0x2,
			kFrozen = 0x3,
			kDebris = 0x4,
			kCount = 0x5,
			kInvalid = 0xFFFF
		};
	};
	static_assert(sizeof(hknpMotionPropertiesId) == 0x02);
}
