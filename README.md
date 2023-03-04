# WIP
This project is a work in progress, it is not meant to be used yet

If you have any contribution to bring, feel free, there is room for improvement in many places

# Todo list:

- Unicode support for ImGui. The translation file should be able to contain extended latin, korean, japanese, russian and arabic characters. Moveset files hould also be able to contain these, and possibly moveset header strings too
- Export tag2
- Movest compression. It should be viable to store movesets in a compressed format: even the fastest compression in 7zip yiels a 50% size reduction, which is nothing to scoff at
- Figure out how to get 0x64 animation sizes in bytes. This would make moveset smallers AND fix bugs with some animations that are not extracted completely (rare, but happens for some fringe ones)
- Moveset rename icon in moveset extraction list
- Finish importing: There is more to it than simply copying the structures, some more alues have to be corrected, look for which in motbinImport.py
- Code injection to force current moveset to stay among loading screens
- **Moveset editor**
- Auto-updater
- Import MOTA