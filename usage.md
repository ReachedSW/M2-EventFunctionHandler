# EventFunctionHandler Usage

## AddEvent

Use `AddEvent` for simple second-based timers.

### Run once after 10 seconds

```cpp
CEventFunctionHandler::instance().AddEvent(
	[this](SArgumentSupportImpl*)
	{
		this->SendNotificationToAll();
	},
	"empire_notice_once",
	10
);
```

### Run every 60 seconds

```cpp
CEventFunctionHandler::instance().AddEvent(
	[this](SArgumentSupportImpl*)
	{
		this->BroadcastScore(true);
	},
	"empire_score_loop",
	60,
	true
);
```

## AddPulseEvent

Use `AddPulseEvent` when you need old `EVENTFUNC`-style behavior.

- Initial delay is given in pulses.
- The callback returns the next pulse delay.
- Return `0` to stop the event.

### Dynamic repeat example

```cpp
DynamicCharacterPtr chPtr;
chPtr = this;

CEventFunctionHandler::instance().AddPulseEvent(
	[chPtr](SArgumentSupportImpl*) -> long
	{
		LPCHARACTER ch = chPtr.Get();
		if (!ch)
			return 0;

		if (ch->IsDead())
			return 0;

		ch->PointChange(POINT_HP, 100);

		return PASSES_PER_SEC(3);
	},
	"char_recovery_dynamic",
	PASSES_PER_SEC(3)
);
```

### One-shot pulse example

```cpp
CEventFunctionHandler::instance().AddPulseEvent(
	[vid = GetVID()](SArgumentSupportImpl*) -> long
	{
		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
		if (!ch)
			return 0;

		ch->ChatPacket(CHAT_TYPE_INFO, "Pulse event triggered.");
		return 0;
	},
	"char_one_shot_pulse",
	PASSES_PER_SEC(1)
);
```

## Notes

- `AddEvent` is second-based.
- `AddPulseEvent` is pulse-based.
- Event names must be unique.
- `RemoveEvent(name)` is safe even if the event does not exist.
- `FindEvent(name)` checks whether an event is currently registered.
- `GetDelay(name)` returns remaining delay in seconds.
