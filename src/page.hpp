#pragma once

#include <map>
#include <string>
#include <list>
#include "xhtml.hpp"
#include "style.hpp"

struct RenderInstruction {
    double x;
    double y; // calculat pentru pdf
    double width;
    double height;
    int z_order; // Nivelul de adâncime (pentru a gestiona suprapunerea)
    XhtmlElement element;
    Style style;
    std::wstring text_content = L"";
    std::wstring renderFunction;
    std::vector<std::wstring> globalVars;
};



// Definitie in fisierul Page.hpp sau un utilitar general
struct PageDimensions {
    double widthPt;
    double heightPt;
};

// Functie utilitara care returneaza harta dimensiunilor
inline const std::map<std::wstring, PageDimensions>& getStandardPageSizes() {
    // Folosim static pentru a ne asigura ca harta este initializata o singura data
    static const std::map<std::wstring, PageDimensions> standardSizes = {
        // Format ISO 216 (A-series, utilizat in mare parte in lume)
        { L"A0",      { 2383.94, 3370.39 } }, // Pentru cei care vor sa tipareasca panouri stradale
        { L"A1",      { 1683.78, 2383.94 } },
        { L"A2",      { 1190.55, 1683.78 } },
        { L"A3",      { 841.89,  1190.55 } },
        { L"A4",      { 595.28,  841.89  } }, // STANDARD-UL EUROPEAN/MONDIAL
        { L"A5",      { 419.53,  595.28  } },

        // Format Nord-American (utilizat in SUA, Canada)
        { L"Letter",  { 612.0,   792.0   } },
        { L"Legal",   { 612.0,   1008.0  } },
        { L"Tabloid", { 792.0,   1224.0  } }, // Sau Ledger

        // Formate de dimensiuni web/ecran (utilizare limitata, dar utile)
        { L"US-Web",  { 576.0,   792.0   } }
    };
    return standardSizes;
}


enum class PageOrientation {
    Portrait,
    Landscape
};

class Page {
private:
    double widthPt;
    double heightPt;

    double marginTop;
    double marginBottom;
    double marginLeft;
    double marginRight;

    int pageNumber;
    std::wstring sizeName;
    PageOrientation orientation;

    std::list<RenderInstruction> renderQueue;

public:
    Page(const std::wstring& size = L"A4",
        PageOrientation orient = PageOrientation::Portrait,
        double defaultMargin = 72.0,
        int number = 1);

    Page(const std::wstring& size,
        PageOrientation orient,
        double mTop, double mRight, double mBottom, double mLeft, int number);


    // Getters
    std::wstring getSizeName() const { return sizeName;  }
    PageOrientation getOrientation() const { return orientation; }
    double getWidth() const;
    double getHeight() const;
    double getContentWidth() const;
    double getContentHeight() const;
    int getPageNumber() const;
    std::list<RenderInstruction>& getRenderQueue() { return renderQueue; }


    double getMarginLeft() const { return marginLeft; }
    double getMarginTop() const { return marginTop; }
    double getMarginRight() const { return marginRight; }
    double getMarginBottom() const { return marginBottom; }



    // Setters
    void setMargins(double top, double right, double bottom, double left);
    void setSize(const std::wstring& size, PageOrientation orient = PageOrientation::Portrait);

    // Stiluri CSS @page
    void applyCssPageRule(const std::map<std::wstring, std::wstring>& properties);
    void print() const;
    void reset();


    void pushBackInstruction(RenderInstruction instr) { renderQueue.push_back(instr); }
    void pushFrontInstruction(RenderInstruction instr) { renderQueue.push_front(instr); }
    void clearInstrucions() { renderQueue.clear(); }
};

