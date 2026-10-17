# Vendored TI MSPM0 SDK files

These files are an unmodified subset of the TI MSPM0 SDK 2.11.00.07
(github.com/TexasInstruments/mspm0-sdk, commit 20807db79aa1), trimmed to what
the MSPM0G3507 AM32 port needs.

| Path | Origin | License (per SDK manifest and file headers) |
|---|---|---|
| `ti/devices/` | `source/ti/devices/` | BSD-3-Clause |
| `ti/driverlib/` | `source/ti/driverlib/` | BSD-3-Clause |
| `CMSIS/Core/Include/` | `source/third_party/CMSIS/Core/Include/` | Apache-2.0 (Arm CMSIS-Core 5.x) |

The SDK's top-level "TI Commercial" license applies only to items the manifest
lists as TI Commercial; none of the files here are in that category.

To refresh: re-copy the same file list from a newer SDK checkout and rebuild.
