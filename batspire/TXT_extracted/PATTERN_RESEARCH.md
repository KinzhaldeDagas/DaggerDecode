# Batspire `TXT_extracted` Pattern Research (Full `.TXT` Coverage)

## Scope
- Folder analyzed: `batspire/TXT_extracted`
- Total files: **255** (`253` `.TXT` + `1` non-`.TXT`)
- Goal: classify **every `.TXT` file** and identify dialogue source-of-truth vs routing assets.

## Source-of-truth for talk data
- `*T.TXT` files are canonical authored dialogue lines (saycodes, NPC lines, replies, checks, actions).
- `*B.TXT` files are base/random dispatch tables that route to talk saycodes.
- `*M.TXT` / `*F.TXT` files are gender-specific routing variants where present.
- Practical order: open `*T` first, then matching `*B`, then `*M/*F`.

## Dialogue pattern summary
- Dialogue bundle files: **160**
- Suffix totals: `T=76`, `B=70`, `M=7`, `F=7`
- Prefix/index variants:
  - `CF` -> 1:BT, 2:BT, 3:BT, 4:BT
  - `CH` -> 1:BT
  - `CV` -> 1:BT
  - `DG` -> 1:BT
  - `DK` -> 1:BT
  - `DL` -> 1:BT
  - `DM` -> 1:FMT
  - `DN` -> 1:FMT
  - `DR` -> 1:BT, 2:BT, 3:FMT, 4:BT, 5:BT, 6:BT
  - `DS` -> 1:BT, 2:BT, 3:BT, 5:BT
  - `DT` -> 3:FMT
  - `FR` -> 1:BT, 2:BT, 9:BT
  - `FS` -> 1:BT
  - `FT` -> 1:BT, 2:BT
  - `GH` -> 1:BT, 2:BT, 3:BT
  - `GV` -> 1:BT, 2:BT
  - `IM` -> 1:BT
  - `JK` -> 1:BT
  - `JM` -> 1:BT, 2:BT
  - `KA` -> 1:BT
  - `MA` -> 1:BT, 2:BT, 3:BT, 4:BT, 5:BT, 6:BT
  - `MD` -> 1:BT
  - `ML` -> 1:BT, 2:BT
  - `PB` -> 1:BT
  - `RL` -> 1:BT
  - `SA` -> 1:BT
  - `SD` -> 1:BT
  - `SK` -> 1:FMT, 2:BT, 3:BT, 5:BT
  - `SM` -> 1:FMT
  - `SN` -> 1:BT
  - `SR` -> 1:BT, 2:BT, 3:BT, 4:BT, 5:BT
  - `SV` -> 1:BT
  - `TA` -> 1:BT
  - `TF` -> 1:BT
  - `VM` -> 1:BT, 2:BT, 3:BT
  - `VT` -> 1:BT
  - `WN` -> 1:BT
  - `WR` -> 1:BT, 2:BT, 3:BT
  - `XM` -> 1:BT
  - `ZN` -> 1:BFMT
  - `ZZ` -> 1:BT

## Full `.TXT` classification index
### Dialogue bundles (*T/*B/*M/*F) (160)
- `CF1B.TXT`, `CF1T.TXT`, `CF2B.TXT`, `CF2T.TXT`, `CF3B.TXT`, `CF3T.TXT`, `CF4B.TXT`, `CF4T.TXT`, `CH1B.TXT`, `CH1T.TXT`, `CV1B.TXT`, `CV1T.TXT`, `DG1B.TXT`, `DG1T.TXT`, `DK1B.TXT`, `DK1T.TXT`, `DL1B.TXT`, `DL1T.TXT`, `DM1F.TXT`, `DM1M.TXT`, `DM1T.TXT`, `DN1F.TXT`, `DN1M.TXT`, `DN1T.TXT`, `DR1B.TXT`, `DR1T.TXT`, `DR2B.TXT`, `DR2T.TXT`, `DR3F.TXT`, `DR3M.TXT`, `DR3T.TXT`, `DR4B.TXT`, `DR4T.TXT`, `DR5B.TXT`, `DR5T.TXT`, `DR6B.TXT`, `DR6T.TXT`, `DS1B.TXT`, `DS1T.TXT`, `DS2B.TXT`, `DS2T.TXT`, `DS3B.TXT`, `DS3T.TXT`, `DS5B.TXT`, `DS5T.TXT`, `DT3F.TXT`, `DT3M.TXT`, `DT3T.TXT`, `FR1B.TXT`, `FR1T.TXT`, `FR2B.TXT`, `FR2T.TXT`, `FR9B.TXT`, `FR9T.TXT`, `FS1B.TXT`, `FS1T.TXT`, `FT1B.TXT`, `FT1T.TXT`, `FT2B.TXT`, `FT2T.TXT`, `GH1B.TXT`, `GH1T.TXT`, `GH2B.TXT`, `GH2T.TXT`, `GH3B.TXT`, `GH3T.TXT`, `GV1B.TXT`, `GV1T.TXT`, `GV2B.TXT`, `GV2T.TXT`, `IM1B.TXT`, `IM1T.TXT`, `JK1B.TXT`, `JK1T.TXT`, `JM1B.TXT`, `JM1T.TXT`, `JM2B.TXT`, `JM2T.TXT`, `KA1B.TXT`, `KA1T.TXT`, `MA1B.TXT`, `MA1T.TXT`, `MA2B.TXT`, `MA2T.TXT`, `MA3B.TXT`, `MA3T.TXT`, `MA4B.TXT`, `MA4T.TXT`, `MA5B.TXT`, `MA5T.TXT`, `MA6B.TXT`, `MA6T.TXT`, `MD1B.TXT`, `MD1T.TXT`, `ML1B.TXT`, `ML1T.TXT`, `ML2B.TXT`, `ML2T.TXT`, `PB1B.TXT`, `PB1T.TXT`, `RL1B.TXT`, `RL1T.TXT`, `SA1B.TXT`, `SA1T.TXT`, `SD1B.TXT`, `SD1T.TXT`, `SK1F.TXT`, `SK1M.TXT`, `SK1T.TXT`, `SK2B.TXT`, `SK2T.TXT`, `SK3B.TXT`, `SK3T.TXT`, `SK5B.TXT`, `SK5T.TXT`, `SM1F.TXT`, `SM1M.TXT`, `SM1T.TXT`, `SN1B.TXT`, `SN1T.TXT`, `SR1B.TXT`, `SR1T.TXT`, `SR2B.TXT`, `SR2T.TXT`, `SR3B.TXT`, `SR3T.TXT`, `SR4B.TXT`, `SR4T.TXT`, `SR5B.TXT`, `SR5T.TXT`, `SV1B.TXT`, `SV1T.TXT`, `TA1B.TXT`, `TA1T.TXT`, `TF1B.TXT`, `TF1T.TXT`, `VM1B.TXT`, `VM1T.TXT`, `VM2B.TXT`, `VM2T.TXT`, `VM3B.TXT`, `VM3T.TXT`, `VT1B.TXT`, `VT1T.TXT`, `WN1B.TXT`, `WN1T.TXT`, `WR1B.TXT`, `WR1T.TXT`, `WR2B.TXT`, `WR2T.TXT`, `WR3B.TXT`, `WR3T.TXT`, `XM1B.TXT`, `XM1T.TXT`, `ZN1B.TXT`, `ZN1F.TXT`, `ZN1M.TXT`, `ZN1T.TXT`, `ZZ1B.TXT`, `ZZ1T.TXT`

### Book records (BKx_yyy.TXT) (58)
- `BK1_992.TXT`, `BK1_993.TXT`, `BK1_994.TXT`, `BK1_995.TXT`, `BK1_996.TXT`, `BK1_997.TXT`, `BK1_998.TXT`, `BK1_999.TXT`, `BK2_001.TXT`, `BK2_002.TXT`, `BK2_003.TXT`, `BK2_004.TXT`, `BK2_005.TXT`, `BK2_006.TXT`, `BK2_007.TXT`, `BK2_008.TXT`, `BK2_009.TXT`, `BK2_010.TXT`, `BK2_011.TXT`, `BK2_013.TXT`, `BK2_014.TXT`, `BK2_015.TXT`, `BK2_016.TXT`, `BK2_017.TXT`, `BK2_018.TXT`, `BK2_019.TXT`, `BK3_020.TXT`, `BK3_021.TXT`, `BK3_022.TXT`, `BK3_023.TXT`, `BK3_024.TXT`, `BK4_025.TXT`, `BK4_027.TXT`, `BK4_028.TXT`, `BK4_029.TXT`, `BK4_030.TXT`, `BK5_026.TXT`, `BK5_031.TXT`, `BK5_032.TXT`, `BK5_033.TXT`, `BK5_034.TXT`, `BK5_035.TXT`, `BK5_036.TXT`, `BK5_037.TXT`, `BK5_038.TXT`, `BK6_039.TXT`, `BK6_040.TXT`, `BK6_041.TXT`, `BK6_042.TXT`, `BK6_043.TXT`, `BK6_044.TXT`, `BK6_045.TXT`, `BK6_046.TXT`, `BK6_047.TXT`, `BK6_048.TXT`, `BK6_049.TXT`, `BK8_050.TXT`, `BK8_051.TXT`

### Book-like standalone (1)
- `BOOK0000.TXT`

### Item/Magic/Sigil tables (6)
- `ITEML2.TXT`, `MAGIC.TXT`, `MG0_GEN.TXT`, `MG2_SPC.TXT`, `SIGILS.TXT`, `SITEML2.TXT`

### Auxiliary tables/lists (5)
- `AMTBL1.TXT`, `AMTBL2.TXT`, `AMTBL3.TXT`, `L2.TXT`, `L5.TXT`

### Bug tracking / issue notes (11)
- `BIG_BUGS.TXT`, `BUGS.TXT`, `BUGSL1.TXT`, `BUGSL2.TXT`, `BUGSL3.TXT`, `BUGSL4.TXT`, `BUGSL5.TXT`, `BUGSL6.TXT`, `BUGSL7.TXT`, `BUGSMULT.TXT`, `LIL_BUGS.TXT`

### Logs and scratch notes (12)
- `ADR.TXT`, `COMLOG.TXT`, `ERRORLOG.TXT`, `JULIAN!!.TXT`, `JULIAN.TXT`, `MEMLOG.TXT`, `NOTE.TXT`, `SPIRE.TXT`, `STUFF.TXT`, `TEMP.TXT`, `TMP.TXT`, `TODO.TXT`

### Unclassified TXT (0)
- _None_

## Non-`.TXT` file in folder
- `TALKCHK.EXE`

## Notes
- This index is intentionally exhaustive for the current folder snapshot and can be regenerated as files change.
