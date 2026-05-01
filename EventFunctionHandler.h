/*
	Sherer 2017-2024 (C) - Revisioned by martysama0134 for c++20 support.
	Feel free to use it on your own.
	But ffs don`t remove this.
*/

#pragma once
#include "utils.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct SArgumentSupportImpl {}; // keep for v1 compatibility

struct StringHash
{
	using is_transparent = void;
	size_t operator()(std::string_view sv) const noexcept
	{
		return std::hash<std::string_view>{}(sv);
	}
};

class CEventFunctionHandler : public singleton<CEventFunctionHandler>
{
public:
	enum class ETimeBase : uint8_t
	{
		Seconds,
		Pulses,
	};

	using EventCallback = std::function<long(SArgumentSupportImpl*)>;

private:
	struct SHeapEntry
	{
		size_t dueAt{};
		uint64_t generation{};
		std::string key;

		bool operator<(const SHeapEntry& rhs) const
		{
			return dueAt > rhs.dueAt;
		}
	};

	struct SFunctionHandler
	{
		EventCallback func;
		ETimeBase timeBase{ETimeBase::Seconds};
		size_t dueAt{};
		long loopTime{};
		uint64_t generation{0};

		void ApplyDelay(const long delay);
		bool IsLooped() const { return loopTime != 0; }
		long GetRemaining(const size_t currentSeconds, const size_t currentPulse) const;
	};

public:
	CEventFunctionHandler() = default;
	virtual ~CEventFunctionHandler() = default; // Destroy() only clears up std containers so no need to explicitly call it here

	void Destroy();
	bool AddEvent(std::function<void(SArgumentSupportImpl*)> func, const std::string_view event_name, const size_t time, const bool loop = false);
	bool AddPulseEvent(EventCallback func, const std::string_view event_name, const long pulseDelay);
	void RemoveEvent(const std::string_view event_name);
	void DelayEvent(const std::string_view event_name, const size_t newtime);
	bool FindEvent(const std::string_view event_name) const;
	DWORD GetDelay(const std::string_view event_name) const;
	void Process();

private:
	std::priority_queue<SHeapEntry> & GetQueue(ETimeBase timeBase);
	const SFunctionHandler* GetHandlerByName(std::string_view event_name) const;
	SFunctionHandler* GetHandlerByName(std::string_view event_name);
	bool AddEventInternal(EventCallback func, std::string_view event_name, long delay, bool loop, ETimeBase timeBase);
	void ScheduleEvent(std::string_view event_name, SFunctionHandler& handler, long delay);
	void ProcessQueue(std::priority_queue<SHeapEntry>& queue, ETimeBase timeBase, size_t now);
	std::unordered_map<std::string, std::unique_ptr<SFunctionHandler>, StringHash, std::equal_to<>> m_event;
	std::priority_queue<SHeapEntry> m_secondsQueue;
	std::priority_queue<SHeapEntry> m_pulseQueue;
};
