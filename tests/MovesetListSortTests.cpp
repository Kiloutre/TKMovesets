#include "MovesetListSort.hpp"
#include <stdexcept>

void RunMovesetListSortTests() {
    auto check=[](bool ok) { if(!ok)throw std::runtime_error("Main-menu moveset sorting failed"); };
    movesetInfo alpha{}, beta{}, duplicate{}, accented{};
    alpha.name="alpha";alpha.origin="T8";alpha.filename=L"C:/a.tkmvst";
    beta.name="Beta";beta.origin="T7";beta.filename=L"C:/b.tkmvst";
    duplicate.name="ALPHA";duplicate.origin="T7";duplicate.filename=L"D:/a.tkmvst";
    accented.name=Helpers::to_utf8(L"\u00c9mile");accented.origin="T8";accented.filename=L"C:/e.tkmvst";
    std::vector<movesetInfo*> source{&beta,&duplicate,&alpha,&accented};
    const auto original=source;
    auto view=MovesetListSort::View(source,nullptr);
    check(source==original && view[0]==&alpha && view[1]==&duplicate && view[2]==&beta);
    check(MovesetListSort::Compare(L"\u00c9mile",L"\u00e9MILE")==0);
    check(MovesetListSort::Compare(L"",L"a")<0 && MovesetListSort::Compare(L"a",L"")>0 &&
        MovesetListSort::Compare(L"",L"")==0);
    ImGuiTableColumnSortSpecs column;
    column.ColumnUserID=MovesetListSort::Game;column.SortDirection=ImGuiSortDirection_Ascending;
    ImGuiTableSortSpecs specs;specs.Specs=&column;specs.SpecsCount=1;
    view=MovesetListSort::View(source,&specs);
    check(view[0]==&duplicate && view[1]==&beta && view[2]==&alpha);
    column.SortDirection=ImGuiSortDirection_Descending;
    view=MovesetListSort::View(source,&specs);
    check(view[0]==&alpha && view[1]==&accented && view[2]==&duplicate);
    column.ColumnUserID=MovesetListSort::Name;
    view=MovesetListSort::View(source,&specs);
    check(view.back()==&duplicate);
    alpha.name="Zulu";
    view=MovesetListSort::View(source,&specs);
    check(view.front()==&alpha);
    source.erase(source.begin());
    view=MovesetListSort::View(source,&specs);
    check(view.size()==3 && std::find(view.begin(),view.end(),&beta)==view.end());
}
