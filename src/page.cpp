#include "Page.hpp"
#include "Style.hpp" // pentru convertCssLengthToPt

#include <algorithm>

Page::Page(const std::wstring& size, PageOrientation orient, double defaultMargin, int number)
    : sizeName(size), orientation(orient), pageNumber(number) {
    setSize(size, orient);
    setMargins(defaultMargin, defaultMargin, defaultMargin, defaultMargin);
}

Page::Page(const std::wstring& size,
    PageOrientation orient,
    double mTop, double mRight, double mBottom, double mLeft, int number) 
    : sizeName(size), orientation(orient), pageNumber(number) {
    setSize(size, orient);
    setMargins(mTop, mRight, mBottom, mLeft);
};

void Page::setSize(const std::wstring& size, PageOrientation orient) {

    const auto& sizes = getStandardPageSizes();
    auto it = sizes.find(size);

    PageDimensions dim = (it != sizes.end()) ? it->second : sizes.at(L"A4");

    if (orient == PageOrientation::Landscape) {
        widthPt = dim.heightPt;
        heightPt = dim.widthPt;
    }
    else {
        widthPt = dim.widthPt;
        heightPt = dim.heightPt;
    }
}

void Page::setMargins(double top, double right, double bottom, double left) {
    marginTop = top;
    marginRight = right;
    marginBottom = bottom;
    marginLeft = left;
}

double Page::getWidth() const { return widthPt; }
double Page::getHeight() const { return heightPt; }
double Page::getContentWidth() const { return widthPt - marginLeft - marginRight; }
double Page::getContentHeight() const { return heightPt - marginTop - marginBottom; }
int Page::getPageNumber() const { return pageNumber; }

void Page::applyCssPageRule(const std::map<std::wstring, std::wstring>& properties) {
    LOG_INFO(L"[PAGE CSS] Aplicare reguli @page");

    for (const auto& [name, value] : properties) {
        LOG_DEBUG(L"[PAGE CSS] " + name + L" = " + value);

        if (name == L"size") {
            std::wstring val = value;
            std::transform(val.begin(), val.end(), val.begin(), ::towlower);

            // Detectăm orientarea
            if (val.find(L"landscape") != std::wstring::npos) {
                orientation = PageOrientation::Landscape;
                val = val.substr(0, val.find(L"landscape"));
            }
            else {
                orientation = PageOrientation::Portrait;
            }

            val.erase(0, val.find_first_not_of(L" \t"));
            val.erase(val.find_last_not_of(L" \t") + 1);

            // Verificăm dacă e un format standard
            if (getStandardPageSizes().count(val)) {
                setSize(val, orientation);
                sizeName = val;
                LOG_INFO(L"[PAGE CSS] Dimensiune standard detectată: " + val);
            }
            else {
                // Încearcă să parsezi dimensiuni personalizate
                double w = 0, h = 0;
                if (swscanf_s(val.c_str(), L"%lfpt %lf", &w, &h) == 2) {
                    widthPt = w;
                    heightPt = h;
                    sizeName = L"custom";
                    LOG_INFO(L"[PAGE CSS] Dimensiune personalizată setată: " + std::to_wstring(w) + L"x" + std::to_wstring(h));
                }
                else {
                    LOG_WARNING(L"[PAGE CSS] Format necunoscut pentru size: " + val);
                }
            }
        }
        else if (name == L"margin") {
            double m = ConvertUtils::convertCssLengthToPt(value);
            setMargins(m, m, m, m);
            LOG_INFO(L"[PAGE CSS] Margini uniforme: " + std::to_wstring(m));
        }
        else if (name == L"margin-top") {
            marginTop = ConvertUtils::convertCssLengthToPt(value);
            LOG_INFO(L"[PAGE CSS] margin-top: " + std::to_wstring(marginTop));
        }
        else if (name == L"margin-right") {
            marginRight = ConvertUtils::convertCssLengthToPt(value);
            LOG_INFO(L"[PAGE CSS] margin-right: " + std::to_wstring(marginRight));
        }
        else if (name == L"margin-bottom") {
            marginBottom = ConvertUtils::convertCssLengthToPt(value);
            LOG_INFO(L"[PAGE CSS] margin-bottom: " + std::to_wstring(marginBottom));
        }
        else if (name == L"margin-left") {
            marginLeft = ConvertUtils::convertCssLengthToPt(value);
            LOG_INFO(L"[PAGE CSS] margin-left: " + std::to_wstring(marginLeft));
        }
    }
}

void Page::print() const {
    LOG_INFO(L"--- [PAGE DUMP] ---");
    LOG_INFO(L"Size Name      : " + sizeName);
    LOG_INFO(L"Orientation    : " + (orientation == PageOrientation::Portrait ? std::wstring(L"Portrait") : std::wstring(L"Landscape")));
    LOG_INFO(L"Page Number    : " + std::to_wstring(pageNumber));
    LOG_INFO(L"Width (pt)     : " + std::to_wstring(widthPt));
    LOG_INFO(L"Height (pt)    : " + std::to_wstring(heightPt));
    LOG_INFO(L"Margin Top     : " + std::to_wstring(marginTop));
    LOG_INFO(L"Margin Right   : " + std::to_wstring(marginRight));
    LOG_INFO(L"Margin Bottom  : " + std::to_wstring(marginBottom));
    LOG_INFO(L"Margin Left    : " + std::to_wstring(marginLeft));
    LOG_INFO(L"Content Width  : " + std::to_wstring(getContentWidth()));
    LOG_INFO(L"Content Height : " + std::to_wstring(getContentHeight()));
    LOG_INFO(L"-------------------");
}

void Page::reset() {
    LOG_INFO(L"[PAGE RESET] Reinitializare completă a obiectului Page.");

    // Dimensiuni implicite pentru A4 Portrait
    sizeName = L"--";
    orientation = PageOrientation::Portrait;
    pageNumber = 1;

    // Dimensiuni A4 în puncte (Portrait)
    widthPt = 0;
    heightPt = 0;

    // Margini implicite (72pt = 1 inch)
    marginTop = 0;
    marginBottom = 0;
    marginLeft = 0;
    marginRight = 0;

    LOG_DEBUG(L"[PAGE RESET] Dimensiuni setate: " +
        std::to_wstring(widthPt) + L"x" + std::to_wstring(heightPt));
    LOG_DEBUG(L"[PAGE RESET] Margini setate: top=" + std::to_wstring(marginTop) +
        L", right=" + std::to_wstring(marginRight) +
        L", bottom=" + std::to_wstring(marginBottom) +
        L", left=" + std::to_wstring(marginLeft));

}


