#pragma once

namespace RE
{
	namespace AIFormulas
	{
		inline float GetBarterValue(float a_baseValue, float a_charisma, bool a_selling, TESObjectREFR* a_refTarget)
		{
			using func_t = decltype(&AIFormulas::GetBarterValue);
			static REL::Relocation<func_t> func{ ID::AIFormulas::GetBarterValue };
			return func(a_baseValue, a_charisma, a_selling, a_refTarget);
		}
	}
}
