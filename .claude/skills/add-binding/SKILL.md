---
name: add-binding
description: Checklist for adding a game action to Kuki's input -- a named, rebindable binding or tick button, its default key, where it is registered and in what order, and how a system reads it. Use whenever a game gains something the player does, or a key is read directly.
---

# Add a binding

Every action is a binding: rebindable in the settings menu's controls, saved in the preferences as
`input.<name>`, and the keyboard's way of doing it even where the mouse is another.

1. **Binding or button.** Read in `Update` (a camera, a menu, a toggle): `RegisterBinding`. Read
   by the rules in `Tick`, or sent to a peer: `RegisterButton`, which is a binding with a bit in
   `GetButtons()`'s mask. There are at most thirty-two buttons, and their order is the protocol --
   two peers must register the same buttons in the same order.
2. **The name.** `"<Game>.<Action>"` (`"Frontier.Demolish"`), a constant in the game's controls
   header (`game/frontier/include/controls.hpp`) so every system reads it by the same name. The
   description is the third argument, in the words the controls page shows ("Pull down the
   building under the cursor").
3. **The default key.** A `Key` alone (`key.hpp`), never a chord: modifiers are the editor's. A
   key belongs to one binding, so a second binding on the same key takes it from the first. One
   key meaning two things in two modes (the frontier's turn keys turn a building in hand, the
   camera otherwise) is one binding read by two systems, not two bindings.
4. **Where.** Registered once, before any system reads it, in the order the controls page should
   list them: the game's `RegisterControls` from its setup (the frontier), or a system's
   `PostStart` for buttons, whose order is their bits (the duel's `DuelSystem::PostStart`).
   Settings is registered first, by the engine.
5. **Reading it.** `app.IsBindingPressed(name)` / `app.IsBindingHeld(name)` in `Update`; in `Tick`,
   the button's bit in `app.GetButtons()`, or a `GestureRecognizer` over the masks. Never a key
   directly, and nothing that takes a callback.
6. A mouse action needs a binding too: the mouse is a second way, never the only one.

Then rebind it in the controls page and check the new key works and survives a restart (the
preferences are written on shutdown).
