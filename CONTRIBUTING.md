# Contributing

PRs are welcome.

If you're changing unlock behavior, please check `RESEARCH.md` first. The Run has a few different unlock systems and it's easy to make unrelated content unlock by accident.

For DLC-only changes, make sure normal progression still works unless the change is specifically for `UnlockAll`.

If you're adding a new address, offset, function, vtable or OfferId, include enough information for someone else to verify it. A game version/hash, instruction bytes, logs or a simple in-game A/B test is usually enough.

## Building

Build `NFSTR_UltimateUnlocker.sln` as **Release | x86**.

The release files are:

- `NFSTR_UltimateUnlocker.asi`
- `NFSTR_UltimateUnlocker.ini`

## Pull requests

Keep changes focused and mention what you tested. If the change affects `UnlockDLC`, it's useful to name at least one normal progression unlock you checked as well.
