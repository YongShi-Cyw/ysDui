# Threading And Lifetime Contract

ysDui uses a single UI-thread ownership model. Unless a public API explicitly says otherwise, an object must be
created, used, and destroyed on the UI thread that owns its host.

## UI-thread objects

The following objects are UI-thread-only:

- `core::Control` trees, including adding and removing children;
- `core::Host`, focus, capture, hover state, event dispatch, themes, and animation clocks;
- `render::Canvas` instances and every paint or text-measurement callback;
- `IEmbeddedHost`, `IPopupHost`, `ILayeredHost`, `IFrameHost`, native text inputs, and native views;
- control, host, theme, menu, popup, input, and accessibility callbacks.

These APIs provide no internal cross-thread serialization. A callback may remove the current control or replace the
host root; callers must not retain or access a raw control pointer after a callback that can mutate the tree.

## Background work

Background threads may produce independent values or task results. They must not read or mutate a control, host,
Canvas, platform host, theme, or callback owned by the UI thread.

`DuiAsyncImageLoader` is the explicit exception for task submission and result transport. Its decoder runs on the
loader's worker thread and may only use thread-safe decoder state. It must not capture UI objects. Callers apply
`Poll()` results to controls on the UI thread. Destroying the loader cancels delivery and joins its worker before the
destructor returns.

### Cross-thread channel: `DuiUiDispatcher`

`core::Host::Dispatcher()` is the only sanctioned way for a background thread to reach the UI. Post a closure from
the worker; it runs on the UI thread inside `Host::PrepareFrame()`, before layout:

```cpp
host.Dispatcher().PostCoalesced(messageId, [view, text] { view->SetContent(text); });
```

- `Post` is a FIFO queue: every task runs, in order.
- `PostCoalesced` keeps only the newest task per key. Use it for streaming deltas — the producer is fast and the
  consumer is expensive, so intermediate values must be replaced rather than queued (queuing them re-lays-out the
  message thousands of times).
- Before applying a final result, call `CancelCoalesced(key)` **if the final result arrives through a different
  channel** (e.g. a completion event that also carries token statistics). Otherwise an older queued delta can run
  **after** the final text and revert it to a stale prefix.
- Preferred: post the final text through the **same** coalesce key as the deltas. Ordering is then automatically
  correct and no cancel is needed.
- `PrepareFrame()` drains and then lays out, so an applied delta is laid out and painted in the same frame.
- `SetPendingHandler` fires when the queue goes from empty to non-empty, and **may be called from the posting
  thread**. Platform hosts must wire it to a wake-up; without one, posted work waits for the next input event and
  the UI looks frozen. Win32 wires it to `InvalidateRect`.
- The wake-up **must be asynchronous and return promptly**. It runs synchronously on the posting thread, and the UI
  thread may be joining that very producer (see the Gallery `ChatStream` teardown, which joins before it cancels the
  coalesce key). A wake-up that blocks on the UI thread — `SendMessage`, an event wait, a lock the UI thread holds —
  deadlocks. Asynchronous invalidation such as `InvalidateRect` is the correct shape.
- Call `Clear()` before destroying a control tree that a pending task can touch. `Drain()` also stops mid-batch if a
  task calls `Clear()`.
- The dispatcher orders work; it does **not** extend object lifetimes. A closure still runs on the UI thread and
  must still not touch objects that no longer exist. Keep a generation counter or `CancelCoalesced` for the case
  where the target went away.

`DuiChatList` is designed to be driven this way: the worker posts a coalesced delta per message, which appears in the
UI thread and sizes the row on the next frame.

## Non-owning capabilities

Controls do not own objects passed to `SetTextInput`, `SetRichTextInput`, `SetPopupHost`, picker, chooser, or similar
capability setters. The provider must outlive every ordinary use of the binding. To replace or detach a provider, call
the corresponding setter while the old provider is still alive. Destruction invalidates stored callbacks without
calling a provider that may already have been destroyed.

`HostRef` and `NativeViewRef` are weak values. They can be copied across value-producing code, but resolving or using
the referenced UI object remains a UI-thread operation.
