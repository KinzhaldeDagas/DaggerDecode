Daggerfall-CS MVP (TEXT.RSC)
===========================

1) Open DaggerfallCS.sln in Visual Studio 2022
2) Build: Debug|x64 (or Release|x64)
3) Run
4) File > Open ARENA2 Folder...
   - Select either the ARENA2 folder directly, or the game root that contains an ARENA2 subfolder.
5) Export > Export TEXT_RSC_Subrecords.csv...
6) Export > Export TEXT_RSC_Tokens.csv...

Notes:
- This MVP focuses on TEXT.RSC ingestion + CS-like browsing + deterministic CSV export.
- Tokenization is MVP-grade (EndOfLine, NewLine, EndOfPage, Font, Color, BookImage, Unknown).
