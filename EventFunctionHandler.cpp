/*
	Sherer 2017-2024 (C) - Revisioned by martysama0134 for c++20 support.
	Feel free to use it on your own.
	But ffs don`t remove this.
*/

#include "stdafx.h"
#include "config.h"
#include "EventFunctionHandler.h"

void CEventFunctionHandler::SFunctionHandler::ApplyDelay(const long delay)
{
	const size_t now = timeBase == ETimeBase::Pulses ? static_cast<size_t>(thecore_pulse()) : static_cast<size_t>(get_global_time());
	dueAt = now + static_cast<size_t>(std::max<long>(1, delay));
	++generation;
}

long CEventFunctionHandler::SFunctionHandler::GetRemaining(const size_t currentSeconds, const size_t currentPulse) const
{
	const size_t now = timeBase == ETimeBase::Pulses ? currentPulse : currentSeconds;
	return dueAt > now ? static_cast<long>(dueAt - now) : 0;
}

void CEventFunctionHandler::Destroy()
{
	m_event.clear();
}

/*
	AddEvent(Function_to_Call, Event_Name, Time, Loop)
	Adding new event.

	Function_to_Call - std::function object containing function called when event appears.
	Event_Name - unique event name. If you accidently provide name of event which already exists, function returns false (if could rewrite current one but i did deny this idea).
	Time - execution time (in sec).
	Loop - repeat the event every Time seconds.
*/
bool CEventFunctionHandler::AddEvent(std::function<void(SArgumentSupportImpl *)> func, const std::string_view event_name, const size_t time, const bool loop)
{
	EventCallback wrappedFunc = [wrapped = std::move(func)](SArgumentSupportImpl* arg) -> long
	{
		wrapped(arg);
		return 0;
	};
	return AddEventInternal(std::move(wrappedFunc), event_name, static_cast<long>(time), loop, ETimeBase::Seconds);
}

bool CEventFunctionHandler::AddPulseEvent(EventCallback func, const std::string_view event_name, const long pulseDelay)
{
	return AddEventInternal(std::move(func), event_name, pulseDelay, false, ETimeBase::Pulses);
}

/*
	RemoveEvent(Event_Name)
	Deleting event if exists.

	Event_Name - unique event name.
*/
void CEventFunctionHandler::RemoveEvent(const std::string_view event_name)
{
	if (auto it = m_event.find(event_name); it != m_event.end())
		m_event.erase(it);
}

/*
	DelayEvent(Event_Name, NewTime)
	Changing existing event time. Disabled for looped events.

	Event_Name - unique event name.
	NewTime - new time.
*/
void CEventFunctionHandler::DelayEvent(const std::string_view event_name, const size_t newtime)
{
	if (auto ptr = GetHandlerByName(event_name); ptr && !ptr->IsLooped() && ptr->timeBase == ETimeBase::Seconds)
		ScheduleEvent(event_name, *ptr, static_cast<long>(newtime));
}

/*
	FindEvent(Event_Name)
	Checking if event exists.

	Event_Name - unique event name.
*/
bool CEventFunctionHandler::FindEvent(const std::string_view event_name) const
{
	return GetHandlerByName(event_name) != nullptr;
}

/*
	GetDelay(Event_Name)
	Returning event delay.

	Event_Name - unique event name.
*/
DWORD CEventFunctionHandler::GetDelay(const std::string_view event_name) const
{
	const auto currentSeconds = static_cast<size_t>(get_global_time());
	const auto currentPulse = static_cast<size_t>(thecore_pulse());
	if (const auto ptr = GetHandlerByName(event_name))
	{
		const long remaining = ptr->GetRemaining(currentSeconds, currentPulse);
		if (remaining <= 0)
			return 0;

		if (ptr->timeBase == ETimeBase::Pulses)
			return static_cast<DWORD>(remaining / std::max(1, passes_per_sec));

		return static_cast<DWORD>(remaining);
	}
	return 0;
}

void CEventFunctionHandler::Process()
{
	if (m_event.empty())
		return;

	const auto currentSeconds = static_cast<size_t>(get_global_time());
	const auto currentPulse = static_cast<size_t>(thecore_pulse());
	ProcessQueue(m_secondsQueue, ETimeBase::Seconds, currentSeconds);
	ProcessQueue(m_pulseQueue, ETimeBase::Pulses, currentPulse);
}

std::priority_queue<CEventFunctionHandler::SHeapEntry>& CEventFunctionHandler::GetQueue(const ETimeBase timeBase)
{
	return timeBase == ETimeBase::Pulses ? m_pulseQueue : m_secondsQueue;
}

bool CEventFunctionHandler::AddEventInternal(EventCallback func, const std::string_view event_name, const long delay, const bool loop, const ETimeBase timeBase)
{
	if (GetHandlerByName(event_name))
		return false;

	auto handler = std::make_unique<SFunctionHandler>();
	handler->func = std::move(func);
	handler->timeBase = timeBase;
	handler->loopTime = loop ? delay : 0;
	handler->ApplyDelay(delay);

	auto [it, inserted] = m_event.emplace(event_name, std::move(handler));
	if (!inserted)
		return false;

	GetQueue(timeBase).push(SHeapEntry{it->second->dueAt, it->second->generation, it->first});
	return true;
}

void CEventFunctionHandler::ScheduleEvent(const std::string_view event_name, SFunctionHandler& handler, const long delay)
{
	handler.ApplyDelay(delay);
	GetQueue(handler.timeBase).push(SHeapEntry{handler.dueAt, handler.generation, std::string(event_name)});
}

void CEventFunctionHandler::ProcessQueue(std::priority_queue<SHeapEntry>& queue, const ETimeBase timeBase, const size_t now)
{
	while (!queue.empty() && queue.top().dueAt <= now)
	{
		const SHeapEntry heapEntry = queue.top();
		queue.pop();

		auto it = m_event.find(heapEntry.key);
		if (it == m_event.end())
			continue;

		SFunctionHandler& handler = *it->second;
		if (handler.timeBase != timeBase || handler.generation != heapEntry.generation || handler.dueAt != heapEntry.dueAt)
			continue;

		const long nextDelay = handler.func(nullptr);

		it = m_event.find(heapEntry.key);
		if (it == m_event.end())
			continue;

		SFunctionHandler& currentHandler = *it->second;
		if (currentHandler.timeBase != timeBase || currentHandler.generation != heapEntry.generation || currentHandler.dueAt != heapEntry.dueAt)
			continue;

		if (nextDelay > 0)
		{
			ScheduleEvent(it->first, currentHandler, nextDelay);
			continue;
		}

		if (currentHandler.IsLooped())
		{
			ScheduleEvent(it->first, currentHandler, currentHandler.loopTime);
			continue;
		}

		m_event.erase(it);
	}
}

const CEventFunctionHandler::SFunctionHandler* CEventFunctionHandler::GetHandlerByName(const std::string_view event_name) const
{
	if (auto it = m_event.find(event_name); it != m_event.end())
		return it->second.get();
	return nullptr;
}

CEventFunctionHandler::SFunctionHandler* CEventFunctionHandler::GetHandlerByName(const std::string_view event_name)
{
	return const_cast<SFunctionHandler*>(std::as_const(*this).GetHandlerByName(event_name));
}
